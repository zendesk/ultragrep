require 'time'
require 'getoptlong'
require 'pp'
require 'socket'
require 'yaml'

module Ultragrep
  def self.usage(config)
    $stderr.puts <<-EOL
    Usage: ultragrep [OPTIONS] [REGEXP ...]
    Options:
        --help, -h                This text
        --config                  Config file location (default: .ultragrep.yml, ~/.ultragrep.yml, /etc/ultragrep.yml)
        --progress, -p            show grep progress to STDERR
        --tail, -t                Tail requests, show matching requests as they arrive
        --type, -l      LOGTYPE   Search type of logs.  available types: #{config['types'].keys.join(',')}
        --perf                    Output just performance information
        --day, -d       DATE      Find requests that happened on this day
        --daysback, -b  COUNT     Find requests from COUNT days ago to now
        --hoursback, -o COUNT     Find requests  from COUNT hours ago to now
        --start, -s     DATETIME  Find requests starting at this date
        --end, -e       DATETIME  Find requests ending at this date
        --host          HOST      Only find requests on this host

    Note about dates: all datetimes are in UTC, and are flexibly whatever ruby's
    Time.parse() will accept.  the format '2011-04-30 11:30:00' will work just fine, if you
    need a suggestion.
    EOL
  end

  DAY = 3600 * 24


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

      @mutex.synchronize {
        to_this_ts = @children_timestamps.values.min
        $stderr.puts("I've searched up through #{Time.at(to_this_ts)}") if @verbose && to_this_ts > 0 && to_this_ts != 2**50
        @all_data.each { |req|
          if req[0] <= to_this_ts
            dump_this << req
          else
            new_data << req
          end
        }
        @all_data = new_data
      }

      STDOUT.write(
        dump_this.sort { |a, b|
          a[0] <=> b[0]
        }.map { |a| a[1] }.join("")
      )

      STDOUT.flush
    end

    def run
      # begin printer thread
      Thread.new {
        while @all_data.size > 0 || !@finish
          sleep 2
          dump_buffer
          #next if all_data.empty?
        end
        dump_buffer
      }
    end

    def add_request(request_array)
      @mutex.synchronize {
        r = format_request(request_array)
        @all_data << [request_array[0], r] if r
      }
    end

    def format_request(request)
      request[1]
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

  class RequestPerfPrinter < RequestPrinter
    def format_request(request_array)
      matches = request_array[1].match(/.*Processing ([^ ]+) .*Completed in (\d+)ms/m)
      if matches
        action = matches[1]
        time = matches[2]
        r = "#{request_array[0]}\t#{action}\t#{time}\n"
      else
        nil
      end
    end
  end

  def self.parse_dates_from_fname(fname)
    fname =~ /(\d+)(\.\w+)?$/
    start_time = Time.parse($1)
    return [start_time.to_i, (start_time.to_i + DAY) - 1]
  end

  def self.collect_files(start_time, end_time, globs, hostfilter)
    all_files = Dir.glob(globs)

    host_files = all_files.inject({}) { |hash, file|
      hostname = file.split("/")[-2]

      next hash if hostfilter && !hostfilter.include?(hostname)

      hash[hostname] ||= []
      f_start_time, f_end_time = parse_dates_from_fname(file)

      hash[hostname] << {:name => file, :start_time => f_start_time, :end_time => f_end_time}
      hash
    }

    host_files.keys.each { |host|
      host_files[host].sort! { |a, b|
        a[:end_time] <=> b[:end_time]
      }


      host_files[host].reject! { |hash|
        hash[:start_time] < start_time && hash[:end_time] < start_time ||
          hash[:end_time] > end_time && hash[:start_time] > end_time
      }
    }

    ret = []
    i = 0

    # have a hash hostname => arrays
    by_start_time = host_files.values.flatten.uniq { |v| v[:name] }.group_by { |v| v[:start_time].to_i }
    by_start_time.keys.sort.map { |k| by_start_time[k].map { |h| h[:name] } }
  end

  def self.parse_time(str)
    now = Time.now
    if str =~ /^\d+$/ && !str =~ /^20/
      time = Time.at(str.to_i)
    else
      time = Time.parse(str)
    end

    # buh, Time.parse won't raise in ruby 1.8
    if now.to_i == time.to_i
      return nil
    end
    time.to_i
  end

  def self.parse_args()
    i = 0

    opts = {:files => [], :regexps => []}

    args = GetoptLong.new(
      [ '--verbose', '-v', GetoptLong::NO_ARGUMENT ],
      [ '--progress', '-p', GetoptLong::NO_ARGUMENT ],
      [ '--help', '-h', GetoptLong::NO_ARGUMENT ],
      [ '--tail', '-t', GetoptLong::NO_ARGUMENT ],
      [ '--type', GetoptLong::REQUIRED_ARGUMENT],
      [ '--config', GetoptLong::REQUIRED_ARGUMENT],
      [ '--perf', GetoptLong::NO_ARGUMENT ],
      [ '--day', '-d', GetoptLong::REQUIRED_ARGUMENT],
      [ '--daysback', '-b', GetoptLong::REQUIRED_ARGUMENT],
      [ '--hoursback', '-o', GetoptLong::REQUIRED_ARGUMENT],
      [ '--start', '-s', GetoptLong::REQUIRED_ARGUMENT],
      [ '--end', '-e', GetoptLong::REQUIRED_ARGUMENT],
      [ '--host', GetoptLong::REQUIRED_ARGUMENT]
    )

    args.each do |opt, arg|
      case opt
      when '--help'
        opts[:do_usage] = true
      when '--daysback'
        back = arg.to_i
        opts[:range_start] = Time.now.to_i - (back * DAY)
      when '--hoursback'
        back = arg.to_i
        opts[:range_start] = Time.now.to_i - (back * 3600)
      when '--day'
        day = parse_time(arg)
        if day.nil?
          $stderr.puts("Incorrectly formatted time: #{day}")
          exit
        end

        opts[:range_start] = day
        opts[:range_end] = day + DAY
      when '--start'
        opts[:range_start] = parse_time(arg)
        if opts[:range_start].nil?
          $stderr.puts("Incorrectly formatted time: #{opts[:range_start]}")
          exit
        end
      when '--end'
        opts[:range_end] = parse_time(arg)
        if opts[:range_end].nil?
          $stderr.puts("Incorrectly formatted time: #{opts[:range_end]}")
          exit
        end
      when '--type'
        opts[:type] = arg
      when '--verbose'
        $stderr.puts("The --verbose option is deprecated and will go away soon, please use -p or --progress instead")
        opts[:verbose] = true
      when '--progress'
        opts[:verbose] = true
      when '--tail'
        opts[:tail] = true
      when '--perf'
        opts[:printer] = RequestPerfPrinter.new(opts[:verbose])
      when '--host'
        opts[:hostfilter] ||= []
        opts[:hostfilter] << arg
      when '--config'
        opts[:config] = arg
      end
    end

    opts[:config] ||= begin
      config_locations = [".ultragrep.yml", "#{ENV['HOME']}/.ultragrep.yml", "/etc/ultragrep.yml"]
      found = config_locations.detect { |fname| File.exist?(fname) }
      abort("Please configure #{config_locations.join(", ")}") unless found
      found
    end
    opts[:config] = YAML.load_file(opts[:config])

    if opts[:do_usage]
      usage(opts[:config])
      exit
    end

    opts[:regexps] += ARGV

    if opts[:regexps].empty?
      usage(opts[:config])
      exit
    end

    opts[:range_start] ||= Time.now.to_i - (Time.now.to_i % DAY)
    opts[:range_end] ||= Time.now.to_i + ((Time.now.to_i % DAY) - 1)

    opts[:files] = []

    opts
  end

  def self.ultragrep(opts)
    # Set idle I/O and process priority, so other processes aren't starved for I/O
    system("ionice -c 3 -p #$$ >/dev/null 2>&1")
    system("renice -n 19 -p #$$ >/dev/null 2>&1")

    days = []
    children_pipes = []
    files = []
    file_lists = nil

    config = opts.fetch(:config)
    default_file_type = config.fetch("default_type")

    file_type = opts.fetch(:type, default_file_type)
    log_path_globs = Array(config.fetch('types').fetch(file_type).fetch('glob'))

    if opts[:tail]
      # gotta fix this before we open source. 
      tail_list = Dir.glob(log_path_globs).map { |f|
        today = Time.now.strftime("%Y%m%d")
        if f =~ /-#{today}$/
          "tail -f #{f}"
        end
      }.compact
      file_lists = [tail_list]
    else
      file_lists = collect_files(opts[:range_start], opts[:range_end], log_path_globs, opts[:hostfilter])
    end

    abort("couldn't find any files matching globs: #{log_path_globs.join(',')}") if file_lists.empty?

    $stderr.puts("Grepping #{file_lists.map { |f| f.join(" ") }.join("\n\n\n")}") if opts[:verbose]

    request_printer = opts[:printer] || RequestPrinter.new(opts[:verbose])
    request_printer.run

    quoted_regexps = opts[:regexps].map { |r| "'" + r.gsub("'", "") + "'" }.join(' ')

    if opts[:verbose]
      $stderr.puts("searching for regexps: #{quoted_regexps} from #{Time.at(opts[:range_start])} to #{Time.at(opts[:range_end])}")
    end

    core = "#{ug_guts} #{file_type} #{opts[:range_start]} #{opts[:range_end]} #{quoted_regexps}"
    file_lists.each { |list|
      if opts[:verbose] 
        formatted_list = list.each_slice(2).to_a.map { |l| l.join(" ") }.join("\n")
        $stderr.puts("searching #{formatted_list}")
      end

      list.each { |file|
        if file =~ /\.gz$/
          f = IO.popen("gzip -dcf #{file} | #{core}")
        elsif file =~ /\.bz2$/
          f = IO.popen("bzip2 -dcf #{file} | #{core}")
        elsif file =~ /^tail/
          f = IO.popen(file + " | #{core}")
        else
          f = IO.popen("cat #{file} | #{core}")
        end
        children_pipes << [f, file]
      }

      threads = []
      children_pipes.each do |pipe, filename|
        request_printer.set_read_up_to(pipe, 0)
      end

      # each thread here waits for child data and then pushes it to the printer thread.
      children_pipes.each do |pipe, filename|
        threads << Thread.new {
          parsed_up_to = nil
          this_request = nil
          while (line = pipe.gets)
            line.encode!('UTF-16', 'UTF-8', :invalid => :replace, :replace => '')
            line.encode!('UTF-8', 'UTF-16')
            if line =~ /^@@(\d+)/
              # timestamp coming back from the child.
              parsed_up_to = $1.to_i

              request_printer.set_read_up_to(pipe, parsed_up_to)
              this_request = [parsed_up_to, "\n# #{filename}"]
            elsif line =~ /^---/
              # end of request
              this_request[1] += line if this_request
              if opts[:tail]

                if this_request
                  STDOUT.write(request_printer.format_request(this_request))
                  STDOUT.flush
                end
              else
                request_printer.add_request(this_request) if this_request
              end
              this_request = [parsed_up_to, line]
            else
              this_request[1] += line if this_request
            end
          end
          request_printer.set_done(pipe)
        }
      end
      threads.map(&:join)
      Process.waitall
    }

    request_printer.finish
  end

  def self.ug_guts
    file = File.expand_path("../../ext/ultragrep/ug_guts", __FILE__)
    `cd #{File.dirname(file)} && make 2>&1` unless File.file?(file) # FIXME horrible hack, build these on gem install
    file
  end

end
