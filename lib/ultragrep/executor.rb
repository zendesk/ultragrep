module Ultragrep
  class Executor
    def initialize(options, config)
      @options = options
      @config = config
    end

    def ug_guts
      File.join(bin_path, 'ug_guts')
    end

    def ug_cat
      File.join(bin_path, 'ug_cat')
    end

    def build_ug_command(*args)
      args
    end

    def build_grep_pipe(file, quoted_regexps)
      command = []

      if file =~ /\.bz2$/
        command += ["bzip2", "-dcf", file]
      elsif file =~ /^tail/
        command << file
      else
        index_dir = @options[:config].index_path(file)
        command += [ug_cat, file, @options[:range_start], index_dir]
      end

    end

    protected
    def debug?
      @options[:debug]
    end
  end
end

