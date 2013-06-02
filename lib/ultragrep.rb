require 'time'
require 'optparse'
require 'pp'
require 'socket'
require 'yaml'

module Ultragrep
  HOUR = 60 * 60
  DAY = 24 * HOUR
  CONFIG_LOCATIONS = [".ultragrep.yml", "~/.ultragrep.yml", "/etc/ultragrep.yml"]

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
      # begin printer thread
      Thread.new do
        while @all_data.size > 0 || !@finish
          sleep 2
          dump_buffer
          #next if all_data.empty?
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
      text
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
        parser.banner = <<-BANNER
          Usage: ultragrep [OPTIONS] [REGEXP ...]

          Dates: all datetimes are in UTC whatever Ruby's Time.parse() accepts.
          For example '2011-04-30 11:30:00'.
        BANNER
        parser.on("--help", "-h", "This text"){ puts parser; exit 0 }
        parser.on("--version", "Show version") do
          require 'ultragrep/version'
          puts "Ultragrep version #{Ultragrep::VERSION}"
          exit 0
        end
        parser.on("--config", "-c FILE", String, "Config file location (default: #{CONFIG_LOCATIONS.join(", ")})") { |config| options[:config] = config }
        parser.on("--progress", "-p", "show grep progress to STDERR") { options[:verbose] = true }
        parser.on("--verbose", "-v", "DEPRECATED") do
          $stderr.puts("The --verbose option is deprecated and will go away soon, please use -p or --progress instead")
          options[:verbose] = true
        end
        parser.on("--tail", "-t", "Tail requests, show matching requests as they arrive") { options[:tail] = true }
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
          options[:range_end] = parse_time(host)
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
      default_file_type = config.fetch("default_type")
      file_type = options.fetch(:type, default_file_type)
      log_path_globs = Array(config.fetch('types').fetch(file_type).fetch('glob'))
      file_list = Dir.glob(log_path_globs)

      file_lists = if options[:tail]
        # TODO fix before we open source.
        tail_list = file_list.map do |f|
          today = Time.now.strftime("%Y%m%d")
          "tail -f #{f}" if f =~ /-#{today}$/
        end.compact
        [tail_list]
      else
        collect_files(file_list, options)
      end

      abort("couldn't find any files matching globs: #{log_path_globs.join(',')}") if file_lists.empty?

      $stderr.puts("Grepping #{file_lists.map { |f| f.join(" ") }.join("\n\n\n")}") if options[:verbose]

      request_printer = options.fetch(:printer)
      request_printer.run

      quoted_regexps = quote_shell_words(options[:regexps])

      if options[:verbose]
        $stderr.puts("searching for regexps: #{quoted_regexps} from #{Time.at(options[:range_start])} to #{Time.at(options[:range_end])}")
      end

      core = "#{ug_guts} #{file_type} #{options[:range_start]} #{options[:range_end]} #{quoted_regexps}"

      children_pipes = []
      file_lists.each do |list|
        if options[:verbose]
          formatted_list = list.each_slice(2).to_a.map { |l| l.join(" ") }.join("\n")
          $stderr.puts("searching #{formatted_list}")
        end

        list.each do |file|
          command = if file =~ /\.gz$/
            "gzip -dcf #{file}"
          elsif file =~ /\.bz2$/
            "bzip2 -dcf #{file}"
          elsif file =~ /^tail/
            "#{file}"
          else
            "cat #{file}"
          end
          pipe = IO.popen("#{command} | #{core}")
          children_pipes << [pipe, file]
        end

        threads = []
        children_pipes.each do |pipe, _|
          request_printer.set_read_up_to(pipe, 0)
        end

        # each thread here waits for child data and then pushes it to the printer thread.
        children_pipes.each do |pipe, filename|
          threads << Thread.new do
            parsed_up_to = nil
            this_request = nil
            while line = pipe.gets
              encode_utf8!(line)
              if line =~ /^@@(\d+)/
                # timestamp coming back from the child.
                parsed_up_to = $1.to_i

                request_printer.set_read_up_to(pipe, parsed_up_to)
                this_request = [parsed_up_to, "\n# #{filename}"]
              elsif line =~ /^---/
                # end of request
                this_request[1] += line if this_request
                if options[:tail]
                  if this_request
                    STDOUT.write(request_printer.format_request(*this_request))
                    STDOUT.flush
                  end
                else
                  request_printer.add_request(*this_request) if this_request
                end
                this_request = [parsed_up_to, line]
              else
                this_request[1] += line if this_request
              end
            end
            request_printer.set_done(pipe)
          end
        end
        threads.each(&:join)
        Process.waitall
      end

      request_printer.finish
    end

    private

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

    def parse_dates_from_fname(fname)
      fname =~ /(\d+)(\.\w+)?$/
      start_time = Time.parse($1)
      return [start_time.to_i, (start_time.to_i + DAY) - 1]
    end

    def collect_files(files, options)
      start_time = options.fetch(:range_start)
      end_time = options.fetch(:range_end)
      host_filter = options[:host_filter]

      host_files = files.inject({}) do |hash, file|
        hostname = file.split("/")[-2]

        next hash if host_filter && !host_filter.include?(hostname)

        hash[hostname] ||= []
        f_start_time, f_end_time = parse_dates_from_fname(file)

        hash[hostname] << {:name => file, :start_time => f_start_time, :end_time => f_end_time}
        hash
      end

      host_files.keys.each do |host|
        host_files[host].sort_by! { |a| a[:end_time] }

        host_files[host].reject! do |hash|
          (hash[:start_time] < start_time && hash[:end_time] < start_time) ||
            (hash[:end_time] > end_time && hash[:start_time] > end_time)
        end
      end

      # have a hash hostname => arrays
      by_start_time = host_files.values.flatten.uniq { |v| v[:name] }.group_by { |v| v[:start_time].to_i }
      by_start_time.keys.sort.map { |k| by_start_time[k].map { |h| h[:name] } }
    end

    def parse_time(string)
      if string =~ /^\d+$/ && string !~ /^20/
        string = "#{Time.at(string.to_i).strftime("%Y-%m-%d %H:%M:%S")} #{Time.now.zone}"
      end
      Time.parse(string).to_i
    end

    def load_config(file)
      file ||= begin
        found = CONFIG_LOCATIONS.map { |f| File.expand_path(f) }.detect { |f| File.exist?(f) }
        abort("Please configure #{CONFIG_LOCATIONS.join(", ")}") unless found
        found
      end
      YAML.load_file(file)
    end

    def ug_guts
      File.expand_path("../../ext/ultragrep/ug_guts", __FILE__)
    end
  end
end
