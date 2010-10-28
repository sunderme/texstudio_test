# latex mode: LaTeX commands (package level)
# dani/2006-02-18
# commands with big Letters and others
\AtBeginDocument{code}
\AtEndDocument{code}
\AtEndOfClass{code}
\AtEndOfPackage{code}
\CheckCommand{cmd}[args][default]{def}
\CheckCommand{cmd}[args]{def}
\CheckCommand{cmd}{def}
\CurrentOption
\DeclareOption*{code}
\DeclareOption{option}{code}
\DeclareRobustCommand
\ExecuteOptions{optionlist}
\IfFileExists{file}{then}{else}
\InputIfFileExists{file}{then}{else}
\LoadClass[optionlist]{class}
\LoadClass[optionlist]{class}[release]
\LoadClass{class}
\LoadClass{class}[release]
\NeedsTeXFormat{format}
\NeedsTeXFormat{format}[release]
\OptionNotUsed
\PackageError{text}
\PackageInfo{text}
\PackageWarning{text}
\PackageWarningNoLine{text}
\PassOptionsToClass{optionlist}{class}
\PassOptionsToPackage{optionlist}{package}
\ProcessOptions
\ProcessOptions*
\ProvidesClass{name}
\ProvidesClass{name}[release]
\ProvidesFile{ma,e}
\ProvidesFile{name}{release}
\ProvidesPackage{name}
\ProvidesPackage{name}[release]
\RequirePackage[optionlist]{package}
\RequirePackage[optionlist]{package}[release]
\RequirePackage{package}
\RequirePackage{package}[release]
# counter, lengths and dimens
\setcounter{counter}{value}
\setlanguage{language}
\setlength{\gnat}{length}
\setpapersize{layout}
\settodepth{\gnat}{text}
\settoheight{\gnat}{text}
\settowidth{\gnat}{text}
\addto
\addtocontents{file}{text}
\addtocounter{counter}{value}
\addtolength{\gnat}{length}
\addtoversion
\addvspace{length}
\deffootnote[width}{indention}{par indention}{definition}
\newcounter{foo}
\newcounter{foo}[counter]
\refstepcounter{counter}
\restorecr
\reversemarginpar
\stepcounter{counter}
\stretch{number}
\usecounter{counter}
\usefont{enc}{family}{series}{shape}
\value{counter}
\newfont{cmd}{fontname}
# counter representative
\thechapter
\theenumi
\theenumiv
\theequation
\thefigure
\thefootnote
\thefootnotemark
\thehours
\theminutes
\thempfn
\thempfootnote
\thepage
\theparagraph
\thepart
\thesection
\thesubparagraph
\thesubsection
\thesubsubsection
\thetable
# boxes
\savebox{cmd}[width][pos]{text}
\savebox{cmd}[width]{text}
\savebox{cmd}{text}
\dashbox{dashlength}(width,height)[position]{text}
\dashbox{dashlength}(width,height){text}
\makebox(width,height)[position]{text}
\makebox(width,height){text}
\makebox[width][position]{text}
\makebox[width]{text}
\usebox{\box}
\raisebox{distance}[extendabove][extendbelow]{text}
\raisebox{distance}[extendabove]{text}
\raisebox{distance}{text}
\newsavebox{\box}
# variables
\belowcaptionskip
\binoppenalty
\bottomfraction
\bottomnumber
\dblfigrule
\dblfloatpagefraction
\dblfloatsep
\dbltextfloatsep
\dbltopfraction
\dbltopnumber
\defaultscriptratio
\defaultscriptscriptratio
\doublerulesep
\emergencystretch
\footnotesep
\footskip
\intextsep
\itemindent
\itemsep
\labelenumi
\labelitemi
\labelitemii
\labelitemiii
\labelitemiv
\labelsep
\labelwidth
\leftmargin
\leftmargini
\leftmarginii
\leftmarginiii
\leftmarginiv
\leftmarginv
\leftmarginvi
\leftmark
\marginparpush
\marginparsep
\marginparwidth
\paperheight
\paperwidth
\tabbingsep
\tabcolsep
\topfigrule
\topfraction
\topmargin
\topnumber
\topsep
\totalheight
\totalnumber
\textfloatsep
\textfraction
\abovecaptionskip
\addpenalty
\arraycolsep
\arrayrulewidth
\arraystretch
\badness
\baselinestretch
\columnsep
\columnseprule
\columnwidth
\evensidemargin
\extracolsep
\fboxrule
\fboxsep
\floatpagefraction
\floatsep
\headheight
\headsep
\height
\partopsep
\parsep
\lineskiplimits
\footheight
\DeclareMathSymbol
