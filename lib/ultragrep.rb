require 'time'
require 'optparse'
require 'pp'
require 'socket'
require 'yaml'

require 'ultragrep/config'

module Ultragrep
  HOUR = 60 * 60
  DAY = 24 * HOUR
  DATE_FROM_FILENAME = /(\d+)(\.\w+)?$/

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
    def format_request(parsed_up_to, text)
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
        parser.on("--progress", "-p", "show grep progress to STDERR") { options[:verbose] = true }
        parser.on("--verbose", "-v", "DEPRECATED") do
          $stderr.puts("The --verbose option is deprecated and will go away soon, please use -p or --progress instead")
          options[:verbose] = true
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
        parser.on("--host HOST", String, "Only find requests on this host") do |host|
          options[:host_filter] ||= []
          options[:host_filter] << host
        end
      end
      parser.parse!(argv)

      if argv.empty?
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

      options[:config] = load_config(options[:config])

      options
    end

    def ultragrep(options)
      lower_priority

      config = options.fetch(:config)
      file_type = options.fetch(:type, config.default_file_type)
      file_lists = file_list(config.log_path_glob(file_type), options)

      request_printer = options.fetch(:printer)
      request_printer.run

      quoted_regexps = quote_shell_words(options[:regexps])
      print_regex_info(quoted_regexps, options) if options[:verbose]

      file_lists.each do |files|
        print_search_list(files) if options[:verbose]

        children_pipes = files.map do |file|
          [worker(file, file_type, quoted_regexps, options), file]
        end

        children_pipes.each do |pipe, _|
          request_printer.set_read_up_to(pipe, 0)
        end

        # each thread here waits for child data and then pushes it to the printer thread.
        children_pipes.map do |pipe, filename|
          worker_reader(filename, pipe, request_printer, options)
        end.each(&:join)

        Process.waitall
      end

      request_printer.finish
    end

    private

    def worker(file, file_type, quoted_regexps, options)
      core = "#{ug_guts} #{file_type} #{options[:range_start]} #{options[:range_end]} #{quoted_regexps}"
      command = if file =~ /\.gz$/
        "gzip -dcf #{file}"
      elsif file =~ /\.bz2$/
        "bzip2 -dcf #{file}"
      elsif file =~ /^tail/
        "#{file}"
      else
        "#{ug_cat} #{file} #{options[:range_start]}"
      end
      IO.popen("#{command} | #{core}")
    end

    def worker_reader(filename, pipe, request_printer, options)
      Thread.new do
        parsed_up_to = nil
        this_request = nil
        while line = pipe.gets
          encode_utf8!(line)
          if line =~ /^@@(\d+)/
            # timestamp coming back from the child.
            parsed_up_to = $1.to_i

            request_printer.set_read_up_to(pipe, parsed_up_to)
            this_request = [parsed_up_to, ["\n# #{filename}"]]
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

    def print_regex_info(quoted_regexps, options)
      $stderr.puts("searching for regexps: #{quoted_regexps} from #{range_description(options)}")
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

    def file_list(globs, options)
      file_list = Dir.glob(globs)

      file_lists = if options[:tail]
        # TODO fix before we open source -- this is a hard-coded file format.
        tail_list = file_list.map do |f|
          today = Time.now.strftime("%Y%m%d")
          "tail -f #{f}" if f =~ /-#{today}$/
        end.compact
        [tail_list]
      else
        filter_and_group_files(file_list, options)
      end

      nothing_found!(globs, options) if file_lists.empty?

      $stderr.puts("Grepping #{file_lists.map { |f| f.join(" ") }.join("\n\n\n")}") if options[:verbose]
      file_lists
    end

    def encode_utf8!(line)
      line.encode!('UTF-16', 'UTF-8', :invalid => :replace, :replace => '')
      line.encode!('UTF-8', 'UTF-16')
    end

    # maybe use shellwords but also not super important
    def quote_shell_words(words)
      words.map { |r| "'" + r.gsub("'", ".") + "'" }.join(' ')
    end

    # Set idle I/O and process priority, so other processes aren't starved for I/O
    def lower_priority
      system("ionice -c 3 -p #$$ >/dev/null 2>&1")
      system("renice -n 19 -p #$$ >/dev/null 2>&1")
    end

    def filter_and_group_files(files, options)
      files = filter_files_by_host(files, options[:host_filter])
      files = filter_files_by_date(files, options.fetch(:range_start)..options.fetch(:range_end))
      files.group_by { |f| f[DATE_FROM_FILENAME, 1] }.values
    end

    def filter_files_by_host(files, host_filter)
      return files unless host_filter
      files.select { |file| host_filter.include?(file.split("/")[-2]) }
    end

    def filter_files_by_date(files, range)
      files.select do |file|
        logfile_date = Time.parse(file[DATE_FROM_FILENAME, 1]).to_i
        range_overlap?(range, logfile_date..(logfile_date + DAY - 1))
      end
    end

    def range_overlap?(a, b)
      a.first <= b.last && b.first <= a.last
    end

    def parse_time(string)
      if string =~ /^\d+$/ && string !~ /^20/
        string.to_i
      else
        Time.parse("#{string} UTC").to_i
      end
    end

    def load_config(file)
      Ultragrep::Config.new(file)
    end

    def ug_guts
      File.expand_path("../../ext/ultragrep/ug_guts", __FILE__)
    end

    def ug_cat
      File.expand_path("../../ext/ultragrep/ug_cat", __FILE__)
    end
  end
end
