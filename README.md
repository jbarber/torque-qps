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
* query the status of jobs, nodes, queues and servers with the same interface

USAGE
=====

    qps, built from e25d268
    
    usage: ./qps [-h] [-s server] [-t jobs|nodes|queues|servers] [-o indent|xml|json|perl|qstat] [-a attr1,attr2] [-f attr3=foo] [jobid1 jobid2 ...]
    
      h : show help
      s : server to connect to
      o : output format, default is indent
      a : job attributes to display ('all' for all attributes)
      f : attributes=value to filter jobs (e.g. -f job_state=R)
      t : type of query, default is jobs
    
      When jobids are given, only these jobs are filtered by the -f argument

FILTERS
=======

Multiple filters can be specified as attribute=value pairs, the attributes are
equal to those listed by the -l option. Multiple filters are AND'd together to
return the union of jobs matching the filter. At the moment only the equality
comparison is permitted. For example, the following:

    qps -f name=FOO -f owner=jbarber@example.com

returns all jobs owned by the user `jbarber@example.com` and called `FOO`.

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
* qstat output is broken when some jobs have fields not present in others (e.g. interactive)
* JSON/Perl output should not quote non-string data types (e.g. true/false/numbers)
