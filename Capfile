require 'bundler/setup'

set(:disable_deploy_features, [:environment_selector, :warn_if_local, :challenge, :tags])

require 'zendesk/deployment'

set :application, 'ultragrep'
set :repository,  'git@github.com:zendesk/ultragrep'
set :rvm_ruby_string, '1.9.3'
set :user, 'zendesk'
set :environment, [:foo]

role :deploy, "logs03.pod1"
role :deploy, "logs04.pod1"
role :deploy, "logs1.sac1"
#role :deploy, "logs2.sac1"

namespace :deploy do
  task :restart do
    run "sudo su root -c \" echo 'exec /data/ultragrep/current/bin/ultragrep $@' > /usr/local/rvm/gems/ruby-1.9.3-p429/bin/ultragrep\""
    run "sudo chmod +x /usr/local/rvm/gems/ruby-1.9.3-p429/bin/ultragrep"
    run "sudo cp -f #{release_path}/ultragrep.yml.example /etc/ultragrep.yml"
  end
end

