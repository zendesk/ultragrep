ultragrep
=========

the grep that greps the hardest.  

Usage
=====
<pre>
    Usage: ultragrep [OPTIONS] [REGEXP ...]
    Options:
        --help, -h                This text
        --config                  Config file location (default: .ultragrep.yml, ~/.ultragrep.yml, /etc/ultragrep.yml)
        --progress, -p            show grep progress to STDERR
        --tail, -t                Tail requests, show matching requests as they arrive
        --type, -l      LOGTYPE   Search type of logs.  available types: #{config['types'].keys.join(',')}
        --perf                    Output just performance information
        --day, -d       DATE      Find requests that happened on this day
        --daysback, -b  COUNT     Find requests from COUNT days ago to now
        --hoursback, -o COUNT     Find requests  from COUNT hours ago to now
        --start, -s     DATETIME  Find requests starting at this date
        --end, -e       DATETIME  Find requests ending at this date
        --host          HOST      Only find requests on this host

    Note about dates: all datetimes are in UTC, and are flexibly whatever ruby's
    Time.parse() will accept.  the format '2011-04-30 11:30:00' will work just fine, if you
    need a suggestion.
      
</pre>

Examples
========

To tail work-server logs for XYZJob
      
      ultragrep -t -l work XYZJob

To look at logs from last two days for support.zendesk.com and radar_token api

      ultragrep -b 2 'support.zendesk.com' radar_token
