;;test like:
;; cd /path/to/xwidgets-emacs-dir
;; make   all&&  src/emacs -q --eval "(progn (load \"`pwd`/lisp/xwidget-test.el\") (xwidget-demo-basic))"


;; you should see:
;; - a gtk button
;; - a gtk toggle button
;; - a gtk slider button
;; - an xembed window(using gtk_socket) showing another emacs instance
;; - an xembed window(using gtk_socket) showing an uzbl web browser if its installed

;;the widgets will move when you type in the buffer. good!

;;there will be redrawing issues when widgets change rows, etc. bad!

;;its currently difficult to give kbd focus to the xembedded emacs,
;;but try evaling the following:

;; (xwidget-set-keyboard-grab 3 1)


;;currently interface is lame:
;; :type 1=button, 2=toggle btn, 3=xembed socket(id will be printed to stdout)
;; obviously these should be symbols

;; :xwidget-id 1, MUST be unique and < 100 !
;; if slightly wrong, emacs WILL CRASH

;; I will continue to hand-wave issues like these until the hard parts are solved.

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; api functions (tentative)
;;  see xwidget.c for more api functions


(defun xwidget-insert (pos id type title width height)
 "insert an xwidget at POS, given ID, TYPE, TITLE WIDTH and HEIGHT.
ID might be removed future-ish"
  (goto-char pos)
  (put-text-property (point) (+ 1 (point)) 'display (list 'xwidget ':xwidget-id id ':type type ':title title ':width width ':height height))
  )


(defun xwidget-resize-at (pos  width height)
  "Resize xwidget at postion POS to WIDTH and HEIGHT.
theres no corresponding resize-id fn yet, because of display property/xwidget id impedance mismatch.
"
  (let*
      ((xwidget-prop (cdr (get-text-property pos 'display)))
       (id (plist-get  xwidget-prop ':xwidget-id)))
    (setq xwidget-prop (plist-put xwidget-prop ':width width))
    (setq xwidget-prop (plist-put xwidget-prop  ':height height))
          
    (put-text-property pos (+ 1 pos) 'display (cons 'xwidget xwidget-prop))
    (message "prop %s" xwidget-prop)
    (message "id %d w %d  h %d" id width height)
    (xwidget-resize id width height);; this is a badly named internal function!
  ))


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; demo functions


(defun xwidget-demo-basic ()
  (interactive)
  (insert "xwidgetdemo<<< a button. another button\n")
  (xwidget-insert (point-min) 1 1 "button" 40  50)
  (xwidget-insert          15 2 2 "toggle" 60  30)
  (xwidget-insert          30 3 3 "emacs"  400 200)
  (xwidget-insert          20 4 4 "slider" 100 50)
  (xwidget-insert          40 5 3 "uzbl"   400 400)
  (define-key (current-local-map) [xwidget-event] 'xwidget-handler-demo-basic)
)


(defun xwidget-demo-single ()
  (interactive)
  (insert "xwidgetdemo<<< a button. another button\n")
  (xwidget-insert (point-min) 1 1 "1" 200 300)
  (define-key (current-local-map) [xwidget-event] 'xwidget-handler-demo-basic)
)

;it doesnt seem gtk_socket_steal works very well. its deprecated.
; xwininfo -int
; then (xwidget-embed-steal 3 <winid>)
(defun xwidget-demo-grab ()
  (interactive)
  (insert "0 <<< grabbed appp will appear here\n")
  (xwidget-insert          1 1 3 "1" 1000 1000)
  (define-key (current-local-map) [xwidget-event] 'xwidget-handler-demo-grab)  
  )

;ive basically found these xembeddable things:
;openvrml
;emacs
;mplayer
;surf
;uzbl

;try the openvrml:
;/usr/libexec/openvrml-xembed 0 ~/Desktop/HelloWorld.wrl


(defun xwidget-handler-demo-basic ()
  (interactive)
  (message "stuff happened to xwidget %S" last-input-event)
  (let*
      ((xwidget-event-type (nth 2 last-input-event))
       (xwidget-id (nth 1 last-input-event)))
    (cond ( (eq xwidget-event-type 'xembed-ready)
            (let*
                ((xembed-id (nth 3 last-input-event)))
              (message "xembed ready  %S" xembed-id)
              ;;will start emacs/uzbl in a xembed socket when its ready
              (cond
               ((eq 3 xwidget-id)
                (start-process "xembed" "*xembed*" (format "%ssrc/emacs" default-directory) "-q" "--parent-id" (number-to-string xembed-id) ) )
               ((eq 5 xwidget-id)
                (start-process "xembed2" "*xembed2*" "uzbl"  "-s" (number-to-string xembed-id)  "http://www.fsf.org" )  )
               
              )
            )))))



(defun xwidget-handler-demo-grab ()
  (interactive)
  (message "stuff happened to xwidget %S" last-input-event)
  (let*
      ((xwidget-event-type (nth 2 last-input-event)))
    (cond ( (eq xwidget-event-type 'xembed-ready)
            (let*
                ((xembed-id (nth 3 last-input-event)))
              (message "xembed ready  %S" xembed-id)
              )
            ))))
(defun xwidget-dummy-hook ()
  (message "xwidget dummy hook called"))

;  (xwidget-resize-hack 1 200 200)

;(xwidget-demo-basic)