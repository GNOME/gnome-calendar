# Some words about how do we work so far #

1. We load calendar sources from Online Accounts.
   (So we'll need a view for adding sources, since I refuse
   to read some gconf key from evolution to load some others.
   Maybe that's will be the way eventually, but well)

2. Using sexp for getting objects between two dates:
   (occur-in-time-range? (make-time "20001116T230000Z") (make-time "20001217T230000Z") "America/New_York")
