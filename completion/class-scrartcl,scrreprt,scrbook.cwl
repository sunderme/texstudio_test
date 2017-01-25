# mode: koma classes (scrartcl,scrreprt,scrbook)
# dani/2006-02-21
\addchap[short title]{title}#L1
\addchap{title}#L1
\addchap*{title}#L1
\addpart[short title]{title}#L0
\addpart{title}#L0
\addpart*{title}#L0
\addsec[short title]{title}#L2
\addsec{title}#L2
\addsec*{title}#L2
\addtokomafont{name}{cmd}#*
\areaset[bcor]{width}{height}#*
\areaset{width}{height}#*
\begin{addmargin}{indent}#*
\begin{addmargin*}{indent}#*
\begin{addmargin}[leftindent]{indent}#*
\begin{addmargin*}[middleindent]{indent}#*
\begin{captionbeside}[short]{title}
\begin{captionbeside}[short]{title}[pos]
\begin{captionbeside}[short]{title}[pos][width]
\begin{captionbeside}{title}
\begin{captionbeside}{title}[pos]
\begin{captionbeside}{title}[pos][width]
\begin{captionbeside}{title}[pos][width][offset]
\begin{labeling}[div]{template}\item
\begin{labeling}{template}\item
\captionabove[entry]{text}
\captionabove{text}
\captionbelow[entry]{text}
\captionbelow{text}
\chapappifchapterprefix{text}#*
\dedication{text}
\deffootnote{indent}{parindent}{definition}#*
\deffootnotemark{definition}#*
\deffootnote[width]{indent}{parindent}{definition}#*
\dictumauthorformat{cmd}#*
\dictum[author]{text}
\dictum{text}
\end{addmargin}#*
\end{addmargin*}#*
\end{captionbeside}
\end{labeling}
\enlargethispage
\extratitle{shorttitle}
\ifpdfoutput{then}{else}#*
\ifthispageodd{true}{false}#*
\ifthispagewasodd..\else..\fi#*
\linespread{factor}#*
\lowertitleback{text}
\maketitle{pagenumber}
\marginline{text}
\markboth#*
\markleft#*
\markright#*
\minisec{title}
\othersectionlevelsformat{level}#*
\publishers{text}
\setbibpreamble{text}
\setcapindent{indent}#*
\setcapindent*{indent}#*
\setcapmargin{indent}#*
\setcapmargin*{indent}#*
\setcapmargin[left]{indent}#*
\setcapmargin*[middle]{indent}#*
\setcapwidth[align]{width}#*
\setcapwidth{width}#*
\setchapterpreamble[pos]{text}#*
\setchapterpreamble[pos][width]{text}#*
\setchapterpreamble{text}#*
\SetDIVList{list}#*
\setindexpreamble{text}#*
\setkomafont{name}{cmd}#*
\setpartpreamble[pos]{text}#*
\setpartpreamble[pos][width]{text}#*
\setpartpreamble{text}#*
\subject{text}
\textsubscript{text}
\textsuperscript{text}
\titlehead{head}
\typearea[bcor]{div}#*
\typearea{div}#*
\uppertitleback{text}
\usekomafont{name}#*
# macros
\appendixmore
\autodot
\backmatter
\capfont#*
\caplabelfont#*
\captionformat#*
\chapapp#*
\chapterformat#*
\chaptermarkformat#*
\chapterpagestyle#*
\contentsname#*
\descfont#*
\dictumwidth#*
\figureformat#*
\frontmatter
\indexpagestyle#*
\listfigurename#*
\listtablename#*
\mainmatter
\partformat#*
\partpagestyle#*
\raggeddictum#*
\raggeddictumauthor#*
\raggeddictumtext#*
\raggedsection#*
\sectfont#*
\sectionmarkformat#*
\setcaphanging#*
\subsectionmarkformat#*
\tableformat#*
\titlepagestyle#*
# variable
\pdfoutput#*
\pdfpageheight#*
\pdfpagewidth#*
# logo
\KOMAScript{}#*
## extend macros
\clearpage
\cleardoublepage
\cleardoublepageusingstyle{pagestyle%special}
\cleardoubleemptypage
\cleardoubleplainpage
\cleardoublestandardpage
\cleardoubleoddusingstyle{pagestyle%special}
\cleardoubleoddemptypage
\cleardoubleoddpage
\cleardoubleoddplainpage
\cleardoubleoddstandardpage
\cleardoubleevenusingstyle{pagestyle%special}
\cleardoubleevenemptypage
\cleardoubleevenpage
\cleardoubleevenplainpage
\cleardoubleevenstandardpage
#include:scrpage2
\defpagestyle{pagestyle}{header}{footer}#s#%pagestyle
\newpagestyle{pagestyle}{header}{footer}#s#%pagestyle
\renewpagestyle{pagestyle}{header}{footer}#s#%pagestyle
\providepagestyle{pagestyle}{header}{footer}#s#%pagestyle
