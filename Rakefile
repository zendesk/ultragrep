require 'bundler/setup'
require 'bundler/gem_tasks'
require 'bump/tasks'

task :default => :build_extensions do
  sh "rspec spec/"
end

task :build_extensions do
  unless system "cd ext/ultragrep && (make clean && make install) > /dev/null"
    raise "Failed to build extension"
  end
end
