require 'bundler/setup'
require 'bundler/gem_tasks'
require 'bump/tasks'

task :default => :cleanup do
  sh "rspec spec/"
end

task :cleanup do
  `make -C ext/ultragrep/ clean`
end
