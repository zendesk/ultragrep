require 'time'
require 'optparse'
require 'pp'
require 'socket'
require 'yaml'
require 'byebug'

require 'ultragrep/config'
require 'ultragrep/log_collector'
require 'ultragrep/remote'

module Ultragrep
  HOUR = 60 * 60
  DAY = 24 * HOUR

  class RequestPrinter
    def initialize(verbose)
      @mutex = Mutex.new
      @all_data = []
      @children_timestamps = {}
      @finish = false
      @verbose = verbose
    end

    def dump_buffer
      dump_this = []
      new_data = []

      @mutex.synchronize do
        to_this_ts = @children_timestamps.values.min || 0 # FIXME : should not be necessary, but fails with -t -p
        $stderr.puts("I've searched up through #{Time.at(to_this_ts)}") if @verbose && to_this_ts > 0 && to_this_ts != 2**50
        @all_data.each do |req|
          if req[0] <= to_this_ts
            dump_this << req
          else
            new_data << req
          end
        end
        @all_data = new_data
      end

      STDOUT.write(dump_this.sort.map(&:last).join)
      STDOUT.flush
    end

    def run
      Thread.new do
        while @all_data.size > 0 || !@finish
          sleep 2
          dump_buffer
        end
        dump_buffer
      end
    end

    def add_request(parsed_up_to, text)
      @mutex.synchronize do
        if text = format_request(parsed_up_to, text)
          @all_data << [parsed_up_to, text]
        end
      end
    end

    def format_request(parsed_up_to, text)
      text.join
    end

    def set_read_up_to(key, val)
      @mutex.synchronize { @children_timestamps[key] = val }
    end

    def set_done(key)
      @mutex.synchronize { @children_timestamps[key] = 2**50 }
    end

    def finish
      @finish = true
      dump_buffer
    end
  end

  class RequestPerformancePrinter < RequestPrinter
    def format_request(parsed_up_to, req)
      text = req.join
      return unless text =~ /.*Processing ([^ ]+) .*Completed in (\d+)ms/m
      action = $1
      time = $2
      "#{parsed_up_to}\t#{action}\t#{time}\n"
    end
  end

  class << self
    def parse_args(argv)
      options = {
        :files => [],
        :range_start => Time.now.to_i - (Time.now.to_i % DAY),
        :range_end => Time.now.to_i,
      }
      key_value = []
      warn_about_missing_quotes_in_time_argument(argv)

      parser = OptionParser.new do |parser|
        parser.banner = <<-BANNER.gsub(/^ {6,}/, "")
          Usage: ultragrep [OPTIONS] [REGEXP ...]

          Dates: all datetimes are in UTC whatever Ruby's Time.parse() accepts.
          For example '2011-04-30 11:30:00'.

          Options are:
        BANNER
        parser.on("--help", "-h", "This text"){ puts parser; exit 0 }
        parser.on("--version", "Show version") do
          require 'ultragrep/version'
          puts "Ultragrep version #{Ultragrep::VERSION}"
          exit 0
        end
        parser.on("--config", "-c FILE", String, "Config file location (default: #{Config::DEFAULT_LOCATIONS.join(", ")})") { |config| options[:config] = config }
        parser.on("--setup-remote", "Setup remote hosts.") { options[:setup_remote] = true }
        parser.on("--progress", "-p", "show grep progress to STDERR") { options[:verbose] = true }
        parser.on("--debug", "output debug logging") { options[:debug] = true }
        parser.on("--not REGEXP", "the next given regular expression's match status should invert") do |regexp|
          options[:not_regexps] ||= []
          options[:not_regexps] << regexp
        end

        parser.on("--tail", "-t", "Tail requests, show matching requests as they arrive") do
          options[:tail] = true
          options[:range_end] = Time.now.to_i + 100 * DAY
        end
        parser.on("--type", "-l TYPE", String, "Search type of logs, specified in config") { |type| options[:type] = type }
        parser.on("--perf", "Output just performance information") { options[:perf] = true }
        parser.on("--day", "-d DATETIME", String, "Find requests that happened on this day") do |date|
          date = parse_time(date)
          options[:range_start] = date
          options[:range_end] = date + DAY - 1
        end
        parser.on("--daysback", "-b  COUNT", Integer, "Find requests from COUNT days ago to now") do |back|
          options[:range_start] = Time.now.to_i - (back * DAY)
        end
        parser.on("--hoursback", "-o COUNT", Integer, "Find requests  from COUNT hours ago to now") do |back|
          options[:range_start] = Time.now.to_i - (back * HOUR)
        end
        parser.on("--start", "-s DATETIME", String, "Find requests starting at this date") do |date|
          options[:range_start] = parse_time(date)
        end
        parser.on("--end", "-e DATETIME", String, "Find requests ending at this date") do |date|
          options[:range_end] = parse_time(date)
        end
        parser.on("--around DATETIME", String, "Find a request at about this time (10 seconds buffer on either side") do |date|
          options[:range_start] = parse_time(date) - 10
          options[:range_end] = parse_time(date) + 10
        end
        parser.on("--host HOST", String, "Only find requests on this host") do |host|
          options[:host_filter] ||= []
          options[:host_filter] << host
        end
      end
      parser.parse!(argv)

      if argv.empty? && !options[:setup_remote]
        puts parser
        exit 1
      else
        options[:regexps] = argv
      end

      options[:printer] = if options.delete(:perf)
        RequestPerformancePrinter.new(options[:verbose])
      else
        RequestPrinter.new(options[:verbose])
      end

      options[:config] = load_config(options[:config], options[:type])

      options
    end

    def ultragrep(options)
      config = options.fetch(:config)

      config.validate!

      if config.remote?
        executor = Ultragrep::Remote.new(options, config)
        if options[:setup_remote]
          executor.setup!
          exit
        end
      else
        executor = Ultragrep::Local.new(options, config)
      end

      regexps =  options[:regexps].map { |r| "+" + r }
      regexps += options[:not_regexps].map { |r| "!" + r } if options[:not_regexps]
      quoted_regexps = quote_shell_words(regexps)

      request_printer = options.fetch(:printer)
      request_printer.run

      print_regex_info(options) if options[:verbose]

      children_pipes = executor.get_children_pipes(quoted_regexps)
      children_pipes.each do |pipe, _|
        request_printer.set_read_up_to(pipe, 0)
      end

      # each thread here waits for child data and then pushes it to the printer thread.
      children_pipes.map do |pipe, prefix|
        worker_reader(prefix, pipe, request_printer, options)
      end.each(&:join)

      ret = Process.waitall
      unless ret.all? { |pid, status| status.success? }
        if config.remote?
          $stderr.puts("trouble running ultragrep.  perhaps you need to run --setup-remote?")
        else
          $stderr.puts("trouble running ultragrep.  perhaps you need compile src directory?")
        end
      end


      request_printer.finish
    end

    private

    def worker_reader(prefix, pipe, request_printer, options)
      filename = ""
      Thread.new do
        parsed_up_to = nil
        this_request = nil
        while line = pipe.gets
          encode_utf8!(line)
          if line =~ /^@@(\d+)/
            # timestamp coming back from the child.
            parsed_up_to = $1.to_i

            request_printer.set_read_up_to(pipe, parsed_up_to)
            this_request = [parsed_up_to, ["\n# #{filename}\n"]]
          elsif line =~ /^@@FILE:(.*)/
            filename = prefix + $1
          elsif line =~ /^---/
            # end of request
            this_request[1] << line if this_request
            if options[:tail]
              if this_request
                STDOUT.write(request_printer.format_request(*this_request))
                STDOUT.flush
              end
            else
              request_printer.add_request(*this_request) if this_request
            end
            this_request = [parsed_up_to, [line]]
          else
            this_request[1] << line if this_request
          end
        end
        request_printer.set_done(pipe)
      end
    end

    def print_regex_info(options)
      msg = "searching for regexps: #{options[:regexps].join(',')}"
      if options[:not_regexps]
        msg += " and not #{options[:not_regexps].join(',')}"
      end
      msg += " from #{range_description(options)}"
      $stderr.puts(msg)
    end

    def range_description(options)
      "#{Time.at(options[:range_start])} to #{Time.at(options[:range_end])}"
    end

    def nothing_found!(globs, options)
      abort("Couldn't find any files matching globs: #{globs.join(',')} from #{range_description(options)}")
    end

    def print_search_list(list)
      formatted_list = list.each_slice(2).to_a.map { |l| l.join(" ") }.join("\n")
      $stderr.puts("searching #{formatted_list}")
    end

    def encode_utf8!(line)
      line.encode!('UTF-16', 'UTF-8', :invalid => :replace, :replace => '')
      line.encode!('UTF-8', 'UTF-16')
    end

    # maybe use shellwords but also not super important
    def quote_shell_words(words)
      words.map { |r| "'" + r.gsub("'", ".") + "'" }.join(' ')
    end

    def parse_time(string)
      if string =~ /^\d+$/ && string !~ /^20/
        string.to_i
      else
        Time.parse("#{string} UTC").to_i
      end
    end

    def load_config(file, file_type)
      Ultragrep::Config.new(file, file_type)
    end

    def ug_guts
      File.expand_path("../../src/ug_guts", __FILE__)
    end

    def ug_cat
      File.expand_path("../../src/ug_cat", __FILE__)
    end

    def warn_about_missing_quotes_in_time_argument(argv)
      sep = "---"
      if found = argv.join(sep)[/\d+-\d+-\d+#{sep}\d+:\d+:\d+/]
        warn "WARN: Put time inside quotes like this '#{found.split(sep).join(" ")}'"
      end
    end
  end
end
