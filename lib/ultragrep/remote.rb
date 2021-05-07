require 'open3'

module Ultragrep
  class Remote
    def initialize(options, config)
      @options = options
      @config = config
      @hosts = config.remote_hosts
      @lua = config.lua
    end

    def setup!
      $stderr.puts("checking remote hosts") if debug?

      @hosts.each do |host|
        $stderr.puts("checking state on '#{host}'") if debug?
        exists = system_dbg("ssh", host, "[ -f ~/.ultragrep_remote/ug_guts ]")
        if !exists
          system_dbg("ssh", host, "mkdir -p ~/.ultragrep_remote") || raise("Couldn't make remote dir on #{host}!")

          src_files = `git ls-files -- src`.split(/\s+/)

          command = ["scp"]
          command += src_files
          command << host + ":.ultragrep_remote/"

          system_dbg(*command) || raise("Couldn't scp source files to #{host}!")

          system_dbg("ssh", host, "cd .ultragrep_remote && make") || raise("Couldn't build source on #{host}!")
        end

        system_dbg("scp", @config.lua, host + ":.ultragrep_remote/") || raise("Coudln't scp #{@config.lua} to #{host}")
      end
    end

    def collect_files
      files = []
      glob = @config.log_path_glob
      @hosts.each do |host|
        glob.each do |g|
          ssh_files = syscall("ssh", host, "ls -1 #{g}").split("\n")
          ssh_files.each do |f|
            files << [host, f]
          end
        end
      end
      files
    end

    def system_dbg(*args)
      $stderr.puts(args.join(' ')) if debug?
      system(*args)
    end

    def syscall(*cmd)
      $stderr.puts(cmd.join(' ')) if debug?
      stdout, stderr, status = Open3.capture3(*cmd)
      status.success? && stdout.slice!(0..-(1 + $/.size)) # strip trailing eol
    end

    private
    def debug?
      @options[:debug]
    end
  end
end
