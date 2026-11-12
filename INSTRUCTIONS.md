1. Create a statically generated website
1. document your progress in AGENTS.md as you go so you may continue the process
   if the session is aborted unexpectedly
1. allowed technologies: javascript, html, css, c
1. use the c language to generate the website files;
1. the website files should be in html, css and js
1. we are using nob.h for the build system; refer to ~/Repos/nob.h for
   documentation
1. you can create a Makefile to group all the commands in the single file; i.e.
   building the site builder, building the site, serving the site locally, etc.
1. no package managers, 0 external packages without requesting and only request
   when absolutely necessary
1. Start with the root page, index.html
1. It should have minimal styling, just padding and margins to make site
   readable on different screen sizes, i.e. responsive web design
1. index.html, contains a list of link to articles/blog in the markdown format,
   i will refer to these documents as entries (archive entries)
1. the entries should be populated in this repo via a git submodule, the target
   submodule repo is git@github:kmemdump/notes; ideally i only want to link to a
   specific folder in this repo; notes/writing/essays {$ENTRIES SRC}
1. the entries pages should be generated with the following format url format:
   /archive/{YYYY}/{MM}/{DD}/{title-slug}, where the date refers to the
   published date, one for each file in $ENTRIES_SRC
1. when generating the entry page: should simply embed the markdown file in a
   markdown renderer with 80 column width
1. when generating the entry the populate it with 2 metadata fields above the
   markdown renderer: published date (use ISO format), derive this date from the
   date the file was last edited in git tracking; tags based the intersect of
   words used in the markdown file and a list of tags in this repo;
   ./assets/tags
