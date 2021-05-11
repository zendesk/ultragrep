require 'yaml'

module Ultragrep
  class Config
    DEFAULT_LOCATIONS = [".ultragrep.yml", "#{ENV['HOME']}/.ultragrep.yml", "/etc/ultragrep.yml"]
    def initialize(config_location, file_type)
      @config_location = config_location
      parse!

      @file_type = file_type || default_file_type
    end

    def find_file!
      if @config_location && !File.exist?(@config_location)
        abort("#{@config_location} not found")
      end
      file = ([@config_location] + DEFAULT_LOCATIONS).compact.detect { |fname| File.exist?(fname) }
      abort("Please configure ultragrep.yml (#{DEFAULT_LOCATIONS.join(", ")})") unless file
      file
    end

    def validate!
      if !types[@file_type]
        $stderr.puts("No such log type: #{@file_type} -- available types are #{types.keys.join(',')}")
        exit 1
      end

      if !types[@file_type]["lua"]
        $stderr.puts("Please configure lua parser file for '#{@file_type}' type")
        exit 1
      end
    end

    def type_data
      types[@file_type]
    end

    def lua
      type_data["lua"]
    end

    def local_lua_path
      return nil unless lua

      if lua.include?('/')
        return lua
      else
        return File.join(File.dirname(__FILE__), "..", "..", "lua", lua)
      end
    end

    def parse!
      @data = YAML.load_file(find_file!)
    end

    def [](val)
      @data[val]
    end

    def to_s
      @data.to_s
    end

    def fetch(*args)
      @data.fetch(*args)
    end

    def default_file_type
      @data.fetch('default_type')
    end

    def primary_log
      type_data.fetch('log')
    end

    def log_path_glob
      Array(type_data.fetch('glob'))
    end

    def types
      raise "Please configure the 'types' section of ultragrep.yml" unless @data["types"]
      @data["types"]
    end

    def available_types
      types.keys
    end

    def index_path
      types[@file_type]['index_path'] || @data['index_path']
    end

    def remote?
      !!remote_hosts
    end

    def remote_hosts
      types[@file_type]['remotes']
    end
  end
end
