module Ultragrep
  class LogCollector
    # this constant is pretty implentation-specific.  fix at will.
    DATE_FROM_FILENAME = /(\d+)(\.\w+)?$/

    def initialize(globs, options)
      @globs, @options = globs, options
    end

    def collect_files
      file_list = Dir.glob(@globs)

      file_lists = if @options[:tail]
        # TODO fix before we open source -- this is a hard-coded file format.
        tail_list = file_list.map do |f|
          today = Time.now.strftime("%Y%m%d")
          "tail -f #{f}" if f =~ /-#{today}$/
        end.compact
        [tail_list]
      else
        filter_and_group_files(file_list)
      end

      return nil if file_lists.empty?

      $stderr.puts("Grepping #{file_lists.map { |f| f.join(" ") }.join("\n\n\n")}") if @options[:verbose]
      file_lists
    end

    def filter_and_group_files(files)
      files = filter_files_by_host(files)
      files = filter_files_by_date(files, @options.fetch(:range_start)..@options.fetch(:range_end))
      files.group_by { |f| f[DATE_FROM_FILENAME, 1] }.values
    end

    def filter_files_by_host(files)
      return files unless @options[:host_filter]
      files.select { |file| @options[:host_filter].include?(file.split("/")[-2]) }
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
  end
end
