qps
===
Like Torque's [qstat](http://docs.adaptivecomputing.com/torque/4-2-6/help.htm#topics/commands/qstat.htm), but better (for given values of better).

Shows the status of cluster jobs in a similar fashion to the way "ps" does for
the system (hence "qps").

Features are:
* Selectable job attributes - only show interesting information
* One-row per job output - make it easy to grep for relevent jobs
* JSON + XML output
* qstat-like output - but columns are sized to not truncate the data

USAGE
=====

    qps, version ...
     
    usage: ./qps [-l] [-h] [-s server] [-j|-x|-q] [-f attr1,attr2] [-o attr1,attr2]
    
      h : show help
      l : list all available job attributes
      s : server to connect to
      j : output in JSON format
      x : output in XML format
      q : output in qstat 'format'
      f : filter jobs (not yet implemented)
      o : job attributes to display

BUILD INSTRUCTIONS
==================

You will need the Torque libraries and header files. These are normally
available from your operating system packages (e.g. `torque-libs` and
`torque-devel` under Red Hat).

You also need a C compilar and the make utility. After you have these
prerequisites simply run:

    make

TODO
====

* Filtering of jobs, e.g.:
  * Show jobs from user "foo"
  * Show jobs from all users apart from "foo"
  * Show jobs from all users matching the pattern 'foo.*'
  * Show jobs with a walltime of more than 10 hours
* Show status of queues and servers?
* Output should be more attractive - for example, at the moment the interactive
  field prints as (null) if it's not set to True by libtorque. This is not safe
  and the (null) should be tested for and printed as "False"
* JSON output should not quote non-string data types (e.g. true/false/numbers)
