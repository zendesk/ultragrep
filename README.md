ultragrep
=========

the grep that greps the hardest.

Setup
=====

Store your logs in this structure:

`/custom/host/name.log-<date> # <date> is 20130102 for 2013-01-02`

Define simple `/etc/ultragrep.yml`
```Yaml
types:
  app:
    glob: "/custom/*/production.log-*"
    format: "app"
default_type: app
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
