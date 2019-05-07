"
" Per-project .vimrc file: Configures Vim per Xastir project standards.
"
" Add these two files to the end of your ~/.vimrc file:
"
"   set exrc
"   set secure
"


" Set compatibility to Vim only
set nocompatible

" Turn on syntax highlighting
syntax on

" Tab equals 2 columns
set tabstop=2
set softtabstop=2

" Insert spaces instead of tab characters
set expandtab

" Control how many columns text is indented with the reindent
" operations (<< and >>) and automatic C-style indentation.
set shiftwidth=2

" When off, a <Tab> always inserts blanks according to 'tabstop' or
" 'softtabstop'.  'shiftwidth' is only used for shifting text left or
" right |shift-left-right|.
set nosmarttab

" Display different types of white spaces.
"set list
"set listchars=tab:#\ ,trail:#,extends:#,nbsp:.


