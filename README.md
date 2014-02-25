ultragrep
=========

ultragrep is a grep tool, written at Zendesk, that works with multiple AND'ed
regular expressions across multi-line requests and across multiple files.

[![Build Status](https://travis-ci.org/zendesk/ultragrep.png?branch=master)](https://travis-ci.org/zendesk/ultragrep)

huh?
====

Some examples:

let's say we have a production.log request that logs like this:

    Rewriting API request from /users/123.xml to /api/v1/users/123.xml
    Processing Api::V1::UsersController#show to xml (for 99.99.99.99 at 2013-06-04 23:55:19) [GET]
      Parameters: {"controller"=>"api/v1/users", "action"=>"show", "id"=>"123", "format"=>"xml"}
    Filter chain halted as [:authenticate_user] rendered_or_redirected.
    API request mode:web, subdomain:foo, lotus:no, mobile:false, time:2, account_id:68745, user:, url:https://foo.zendesk.com/api/v1/users/123.xml
    Completed in 7ms (View: 1, DB: 0) | 401 Unauthorized [https://foo.zendesk.com/api/v1/users/123.xml]

The following ultragrep commands go like the following:

    ultragrep foo.zendesk.com                 -> match
    ultragrep foo.zendesk.com frozzbozzle     -> no match, foo.zendesk.com is there but frozzbozzle is not
    utlragrep foo.zendesk.com UsersController -> match, both terms were satisfied (though on different lines)
    ultragrep api/v1/users/\d+\.xml           -> full perl-compatible-regular-expression support

Setup
=====

Store your logs in this structure:

`/custom/host/name.log-<date> # <date> is 20130102 for 2013-01-02`

define `/etc/ultragrep.yml`:

```Yaml
types:
  app:
    glob: "/custom/*/production.log-*"
    format: "app"
default_type: app
```

for rails, it's highly advised to use ActiveSupport::BufferedLogger for your web requests, so as to prevent them
from interleaving.  There's ways to pick out web requests from interleaved logs, but it's not pretty.


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

Examples
========

To tail work-server logs for XYZJob

```Bash
ultragrep -t -l work XYZJob
```

To look at logs from last two days for 'host.com' and foobar

```Bash
ultragrep -b 2 host.com foobar
```

## TODO
 - should not be necessary to ship pcre headers, but it's broken on OSX Mavericks
 - Search in an array , and herarical key-value  for JSON logs

## Copyright and license

Copyright 2013 Zendesk

Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software distributed under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the License for the specific language governing permissions and limitations under the License.
