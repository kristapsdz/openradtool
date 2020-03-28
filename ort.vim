syn keyword kwbpCmd roles
syn keyword kwbpCmd bits
syn keyword kwbpCmd enum
syn keyword kwbpCmd struct
syn keyword kwbpCmd item
syn keyword kwbpCmd field
syn keyword kwbpCmd iterate
syn keyword kwbpCmd list
syn keyword kwbpCmd search
syn keyword kwbpCmd update
syn keyword kwbpCmd insert
syn keyword kwbpCmd delete

syn keyword kwbpInnerCmd int
syn keyword kwbpInnerCmd name
syn keyword kwbpInnerCmd role
syn keyword kwbpInnerCmd comment
syn keyword kwbpInnerCmd jslabel
syn keyword kwbpInnerCmd isunset
syn keyword kwbpInnerCmd isnull

syn region kwbpLiteral start=/\v"/ skip=/\v\\./ end=/\v"/
syn match kwbpComment '#.*$'

hi def link kwbpCmd		Statement
hi def link kwbpInnerCmd	Type
hi def link kwbpLiteral		Constant
hi def link kwbpComment		Comment
