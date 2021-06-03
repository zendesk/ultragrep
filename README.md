ultragrep
=========


Before you can afford to ship your logs to someone else.  Before you pay some
ginourmous company many dollars to host your logs on a highly available whizbang
streaming service.  Before someone on your SRE team decides that putting every
web request into a search index is a scalable idea.  Before that, you've
got some logs on some servers.  Ultragrep is the tool for that searching those.

Ultragrep is a multi-host, multi-line-aggregating grep tool.  It's an easy way
to search for a web request that happened at some point in time on your service.

huh?
====

Let's say I have a few EC2 nodes hosting my little rails startup.  A customer
("customer@foo.com) reports that they tried do invite someone to the site, but
something went wrong.  Armed with their email and an approximate time, I
attempt to save the world via log-diving:

```
ssh p1.gizmo.com
p1.gizmo.com:~$ cd /var/log
p1.gizmo.com:~$ ls -l gizmo.log*
-rw-r----- 1 syslog gizmo 2869792 May 17 10:32 gizmo.log
-rw-r----- 1 syslog gizmo  499776 May 14 23:59 gizmo.log-20210515.gz
-rw-r----- 1 syslog gizmo  510686 May 15 23:59 gizmo.log-20210516.gz
-rw-r----- 1 syslog gizmo 6513979 May 16 23:59 gizmo.log-20210517
```

It's in one of these files, let's hope the request didn't get rotated already...

```
p1.gizmo.com:/var/log$ grep 'customer@foo.com' gizmo.log
gizmo@tms1:/var/log$ grep 'example.com' gizmo.log
I, [2021-05-17T10:34:50.111844 #1]  INFO -- : [31a8df9e-9236-403c-ba9e-410b95189e51] Started GET "/investors/search.json?email=customer@example.com" for 98.37.23.48 at 2021-05-17 10:34:50 +0000
I, [2021-05-17T10:35:04.150729 #1]  INFO -- : [1cf1d635-28d7-46af-b9a3-83ceef6f192c]   Parameters: {"subject"=>"", "body"=>"Hey Mr. Tamborine Man, \n\nPlay a song for me.", "to_email"=>"customer@example.com", "firstname"=>"Mr.", "lastname"=>"Customer", "mail_type"=>"invitation_email", "authenticity_token"=>"6R88ApHFe6EGQSAHLXRBCdMf3ncjO9LQjclVzdPJgZ4BiztJtjPWkcnXhDj/e5jNUuj6PVP+jpPkS7L1Ia5L0g==", "communication"=>{"subject"=>"", "body"=>"Hey Mr. Tamborine Man, \n\nPlay a song for me."}}
I, [2021-05-17T10:35:06.277901 #1]  INFO -- : [4d2a88dc-b6b6-45ad-8ea8-5d38da87cfac]   Parameters: {"authenticity_token"=>"6R88ApHFe6EGQSAHLXRBCdMf3ncjO9LQjclVzdPJgZ4BiztJtjPWkcnXhDj/e5jNUuj6PVP+jpPkS7L1Ia5L0g==", "invitation"=>{"role"=>"prospect", "users"=>[{"email"=>"customer@example.com", "firstname"=>"Mr.", "lastname"=>"Customer"}], "message"=>"Hey Mr. Tamborine Man, \r\n\r\nPlay a song for me."}}
```

There's three different requests that mention this email, which one do we want??

```
p1.gizmo.com:/var/log$ grep '4d2a88dc-b6b6-45ad-8ea8-5d38da87cfac' gizmo.log
mainstage@tms1:/var/log$ grep 4d2a88dc-b6b6-45ad-8ea8-5d38da87cfac gizmo.log
I, [2021-05-17T10:35:06.276065 #1]  INFO -- : [4d2a88dc-b6b6-45ad-8ea8-5d38da87cfac] Started POST "/investors/invite" for 98.37.23.48 at 2021-05-17 10:35:06 +0000
I, [2021-05-17T10:35:06.277660 #1]  INFO -- : [4d2a88dc-b6b6-45ad-8ea8-5d38da87cfac] Processing by InvestorsController#post_invite as HTML
I, [2021-05-17T10:35:06.277901 #1]  INFO -- : [4d2a88dc-b6b6-45ad-8ea8-5d38da87cfac]   Parameters: {"authenticity_token"=>"6R88ApHFe6EGQSAHLXRBCdMf3ncjO9LQjclVzdPJgZ4BiztJtjPWkcnXhDj/e5jNUuj6PVP+jpPkS7L1Ia5L0g==", "invitation"=>{"role"=>"prospect", "users"=>[{"email"=>"customer@example.com", "firstname"=>"Mr.", "lastname"=>"Customer"}], "message"=>"Hey Mr. Tamborine Man, \r\n\r\nPlay a song for me."}}
D, [2021-05-17T10:35:06.280922 #1] DEBUG -- : [4d2a88dc-b6b6-45ad-8ea8-5d38da87cfac]   Company Load (0.7ms)  SELECT `companies`.* FROM `companies` WHERE `companies`.`subdomain` = 'gizmo' LIMIT 1
D, [2021-05-17T10:35:06.283277 #1] DEBUG -- : [4d2a88dc-b6b6-45ad-8ea8-5d38da87cfac]   User Load (0.7ms)  SELECT `users`.* FROM `users` WHERE `users`.`id` = 1 ORDER BY `users`.`id` ASC LIMIT 1

```

Ok, we found it.  Took us 3 commands, but could have taken 10, if we hit the
wrong file, couldn't tell which request it was, etc..  Here's the same session
with ultragrep.


```
devs_laptop:~ $ ultragrep -o 2 'customer@example.com' POST invite
# s1.gizmo.cc:/var/log/gizmo.log
[2021-05-17T10:35:06.276065 #1]  INFO -- : [4d2a88dc-b6b6-45ad-8ea8-5d38da87cfac] Started POST "/investors/invite" for 98.37.23.48 at 2021-05-17 10:35:06 +0000
[2021-05-17T10:35:06.277660 #1]  INFO -- : [4d2a88dc-b6b6-45ad-8ea8-5d38da87cfac] Processing by InvestorsController#post_invite as HTML
[2021-05-17T10:35:06.277901 #1]  INFO -- : [4d2a88dc-b6b6-45ad-8ea8-5d38da87cfac]   Parameters: {"authenticity_token"=>"6R88ApHFe6EGQSAHLXRBCdMf3ncjO9LQjclVzdPJgZ4BiztJtjPWkcnXhDj/e5jNUuj6PVP+jpPkS7L1Ia5L0g==", "invitation"=>{"role"=>"prospect", "users"=>[{"email"=>"customer@example.com", "firstname"=>"Mr.", "lastname"=>"Customer"}], "message"=>"Hey Mr. Tamborine Man, \r\n\r\nPlay a song for me."}}
...
[2021-05-17T10:35:06.395823 #1]  INFO -- : [4d2a88dc-b6b6-45ad-8ea8-5d38da87cfac] Completed 302 Found in 118ms (ActiveRecord: 34.5ms | Allocations: 21303)
--------------------------------------------------------------------------------
```

What I want you to see here is how we passed not one but *three* search terms
to ultragrep.  The command line we gave (`-o 2 'customer@example.com' POST
invite`) says "show me requests from 2 hours til now that contain the terms
"customer@example.com", "POST", and "invite".  Each of those terms must appear
in the request, but they can appear on any line.

Setup
=====


Your hosts that contain log files will need the following:

```
ruby (on the host initiating the grep)
make
pkg-config
gcc
liblua5.2-dev
zlib1g-dev
```

Setup ~/ultragrep.yml on your laptop:

```Yaml
types:
  app:
    log: "/var/log/app.log"
    glob: "/var/log/app.log*"
    lua: newline.lua
    remotes:
      - host1.foo.com
      - host2.foo.com
default_type: app
```

let ultragrep setup some software on the remote hosts:

```
bin/ultragrep --setup-remote
```

Grep!  The first grep may be slow as ultragrep builds indexes.

```
bin/ultragrep -o 3 search_term
```


Usage
=====

<!-- copy paste ./bin/ultragrep -h result -->
```
Usage: ultragrep [OPTIONS] [REGEXP ...]

Dates: all datetimes are in UTC whatever Ruby's Time.parse() accepts.
For example '2011-04-30 11:30:00'.

Options are:
    -h, --help                       This text
        --version                    Show version
    -c, --config FILE                Config file location (default: .ultragrep.yml, ~/.ultragrep.yml, /etc/ultragrep.yml)
    -p, --progress                   show grep progress to STDERR
    -v, --verbose                    DEPRECATED
    -t, --tail                       Tail requests, show matching requests as they arrive
    -l, --type TYPE                  Search type of logs, specified in config
        --perf                       Output just performance information
    -d, --day DATETIME               Find requests that happened on this day
    -b, --daysback  COUNT            Find requests from COUNT days ago to now
    -o, --hoursback COUNT            Find requests  from COUNT hours ago to now
    -s, --start DATETIME             Find requests starting at this date
    -e, --end DATETIME               Find requests ending at this date
        --host HOST                  Only find requests on this host
    -k  --key                        Searched for a given key in a JSON logs.

Note about dates: all datetimes are in UTC, and are flexibly whatever ruby's
Time.parse() will accept.  the format '2011-04-30 11:30:00' will work just fine, if you
need a suggestion.
```

## Copyright and license

Copyright 2013 Zendesk
Copyright 2021 Ben Osheroff

Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software distributed under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the License for the specific language governing permissions and limitations under the License.
