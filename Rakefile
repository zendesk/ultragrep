require 'bundler/setup'
require 'bundler/gem_tasks'
require 'bump/tasks'

task :default => :build_extensions do
  sh "rspec spec/"
end

task :build_extensions do
  unless system "cd ext/ultragrep && (make clean && make install) > /dev/null 2>&1"
    raise "Failed to build extension"
  end
end

task :vendor => :build do
  begin Dir.mkdir("./vendor/cache") rescue Errno::EEXIST end
  File.rename("./pkg/ultragrep-0.0.0.gem", "vendor/cache/ultragrep-0.0.0.gem")
end
