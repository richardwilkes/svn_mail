# svn_mail

Simple helper to send email on svn commits.

From the help:

```
svn_mail
Copyright (c) 2005 by Richard A. Wilkes
All rights reserved worldwide

Usage: svn_mail <svn repository path> <revision> <from email> <dir prefix=subject prefix=to email>...

There must be one or more <dir prefix=subject prefix=to email> specifications.
If 'dir prefix' is '-', it matches all directories. The first one to match wins.

Example:

svn_mail /svn 1 no_reply@sample.com '-=[svn:mine]=svn_users@sample.com'

would send mail to svn_users@sample.com for all check-ins, and would prefix
the mail with '[svn:mine]'.
```
