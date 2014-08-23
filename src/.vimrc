"
" This file is intended to make 'vim' better match the style of emc2 source
" files, and also to enable features like full-text searching of the source
" tree.  You can use it by setting 'exrc' in your ~/.vimrc (but see the
" security warning at :help 'exrc before you do so) or by copying these lines
" into your ~/.vimrc (but then they will affect all files you edit with vim)
"

augroup filetypedetect
 au! BufRead,BufNewFile *.comp setfiletype c
augroup END

augroup emc2
 " Remove all prior emc2 autocommands
 au!
 autocmd FileType *     set formatoptions=tcql nocindent sts=4 et sw=4 comments&
 autocmd FileType c,cpp set formatoptions=croql cindent sts=4 noet sw=4  comments=sr:/*,mb:*,el:/*,://
 autocmd FileType py    set formatoptions=croql nocindent sts=4 et sw=4 comments=b:#
augroup END

" This assumes the working directory is emc2/src which is also required for
" :make and :tag to work
set grepprg=../scripts/swish

