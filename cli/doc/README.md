### The ABC-CLI documentation

Please adjust the abc-cli.pod file, the abc-cli-bash-completion.sh and regenerate the man and html files if you change the CLI.

#### pod2man
Create a new man page:
```
  pod2man cli/doc/abc-cli.pod cli/doc/abc-cli.1
```
View the man page:   man cli/doc/abc-cli.1

See: http://perldoc.perl.org/pod2man.html

#### pod2html
Create a new HTML page:
```
  pod2html --infile=cli/doc/abc-cli.pod --outfile=cli/doc/abc-cli.html --noindex
```
See: http://perldoc.perl.org/pod2html.html
