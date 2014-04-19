Shell implementation in unix
--------------------
--------------------

Description
-------------------
Implementation of shell terminal in unix

First Execute: make
Then the following:

Options implemented:

1)

All the basic Shell commands (ls,clear,vi,etc)

2)

Multiple Pipes with redirection to output

3)

change directory (cd), pwd

4)

export environment variables
syntax: export <name>=<value>

5)

Echo environment variables
syntax: echo $name

6)

Redirection operators (at the end)

7)

Background--> & (at the end)
Foreground--> fg

8)

History

!<num>  ---> execute the command pointed by that line number in history
!-<num> ---> execute the command pointed by that line number from below in history
!<pattern>     ---> execute the last command matched to the input pattern 

9)

Set the required environment variables in "shelrc" configuration file




