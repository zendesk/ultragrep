module Ultragrep
  DEFAULT_CONFIG_LOCATIONS = [".ultragrep.yml", "#{ENV['HOME']}/.ultragrep.yml", "/etc/ultragrep.yml"]
  class Config
    def initialize(config_location)
      @config_location = config_location
      parse!
    end

    def find_file!
      if @config_location && !File.exist?(@config_location)
        abort("#{@config_location} not found")
      end
      file = ([@config_location] + DEFAULT_CONFIG_LOCATIONS).compact.detect { |fname| File.exist?(fname) }
      abort("Please configure ultragrep.yml (#{DEFAULT_CONFIG_LOCATIONS.join(", ")})") unless file
      file
    end

    def parse!
      @data = YAML.load_file(find_file!)
    end

    def [](val)
      @data[val]
    end

    def fetch(*vals)
      if vals.size == 1
        @data.fetch(vals.first)
      else
        @data.fetch(*vals)
      end
    end

    def default_file_type
      @data.fetch('default_type')
    end

    def log_path_glob(type)
      Array(types.fetch(type).fetch('glob'))
    end

    def types
      raise "Please configure the 'types' section of ultragrep.yml" unless @data["types"]
      @data["types"]
    end

    def available_types
      types.keys
    end
  end
end
