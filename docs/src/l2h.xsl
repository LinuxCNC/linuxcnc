<?xml version="1.0"?>
<!--
This is l2h, a converter from lyx to html
Copyright 2007 Jeff Epler <jepler@unpythonic.net>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
-->

<xsl:stylesheet
    xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
    version="1.0">

<xsl:output method="html"
    media-type="text/html"
    doctype-public="-//W3C//DTD HTML 4.01//EN"
    doctype-system="http://www.w3.org/TR/html4/strict.dtd"
    indent="no"
    encoding="ASCII"/>

<xsl:param name="docname"/>

<xsl:template match="/">
    <HTML>
    <HEAD>
    <TITLE>
	<xsl:choose>
	    <xsl:when test="//layout[@class='Title']">
		<xsl:value-of select="//layout[@class='Title']"/>
	    </xsl:when>
	    <xsl:otherwise>
		<xsl:value-of select="document('docs.xml')//doc[@name=$docname]/@title"/>
	    </xsl:otherwise>
	</xsl:choose>
    </TITLE>
    <STYLE TYPE="text/css">
:target { background: #DEF !important;  }
h1, h2 { background: #c0c0f0; }
h1, h2, h3, h4, h5 { border-bottom: 2px solid #8080c0; color: black; }

div.nav { float: right; background: #ffffff; }

dt { font-weight: bold; }
pre { margin-left: 4ex; auto; color: black; padding: 1ex; border-left: 2px solid #8080c0; }
div.float { text-align: center; margin: 2ex; padding: 1ex; }
div.float span.caption { display: block; margin: 1em; }
.typewriter { font-family: monospace; }

table { border-collapse: collapse; margin-left: auto; margin-right: auto; }
.alignment_center { text-align: center; }
.topline { border-top: 1px solid black; }
.bottomline { border-bottom: 1px solid black; }
.leftline { border-left: 1px solid black; }
.rightline { border-right: 1px solid black; }
.v_top { vertical-align: baseline; }
.a_center { text-align: center; }
.a_left { text-align: left; }
.a_right { text-align: right; }

.block { display: block }
.blockformula { display: block; text-align: center }

.f_typewriter { font-family: monospace; }
.noun { font-variant: small-caps; }
.s_bold { font-weight: bold; }

.clist { -moz-column-width: 40ex; -moz-column-gap: 4ex }
.nclist { -moz-column-width: 20ex; -moz-column-gap: 4ex } 
.nclist li { list-style-type: none; text-indent: -.5ex; }
.toc li { list-style-type: none; }
.toc li a { display: block; border: 1px solid transparent; text-indent: -1ex; }

    </STYLE>
    </HEAD>
    <BODY>
    <DIV class="nav">
        <xsl:if test="document('docs.xml')//doc[@name=$docname]/preceding-sibling::*[position()=1]">
            <A HREF="{document('docs.xml')//doc[@name=$docname]/preceding-sibling::*[position()=1]/@name}.html" TITLE="Previous: {document('docs.xml')//doc[@name=$docname]/preceding-sibling::*[position()=1]/@title}">[&lt;-]</A><xsl:text> </xsl:text>
        </xsl:if>
        <A HREF="index.html" TITLE="Up: Documentation Index" >[^]</A><xsl:text> </xsl:text>
        <xsl:if test="document('docs.xml')//doc[@name=$docname]/following-sibling::*[position()=1]">
            <A HREF="{document('docs.xml')//doc[@name=$docname]/following-sibling::*[position()=1]/@name}.html" TITLE="Next: {document('docs.xml')//doc[@name=$docname]/following-sibling::*[position()=1]/@title}">[-&gt;]</A>
        </xsl:if>
    </DIV>
    <xsl:apply-templates/>
    <xsl:if test="//footnote">
	<H3>Footnotes</H3>
	<xsl:apply-templates select="//footnote" mode="endlist"/>
    </xsl:if>
    </BODY>
    </HTML>
</xsl:template>

<xsl:template match="printindex">
    <xsl:if test="//index">
	<H3>Index</H3>
	<UL class="nclist">
	<xsl:for-each select="//index">
	    <xsl:sort select="@lcterm"/>
	    <xsl:apply-templates select="." mode="endlist"/>
	</xsl:for-each>
	</UL>
    </xsl:if>
</xsl:template>

<xsl:template match="footnote">
    <xsl:variable name="inc">
	<xsl:number level="any" count="footnote"/>
    </xsl:variable>			
    <sup><a href="#{$inc}" name="f{$inc}">
    <xsl:value-of select="$inc"/></a></sup>
</xsl:template>

<xsl:template match="footnote" mode="endlist">
    <p>
    <xsl:variable name="incr">
	<xsl:number level="any" count="footnote"/>
    </xsl:variable>		
    <a name="{$incr}"><xsl:value-of select="$incr"/></a>
    &#xa0;
    <xsl:value-of select="."/>
    <xsl:text> </xsl:text>
    <a href="#f{$incr}">back</a>
    </p>
</xsl:template>	

<xsl:template match="toc">
    <xsl:if test="tocentry">
	<H1>Table of Contents</H1>
	<UL CLASS="toc clist">
	<xsl:apply-templates/>
	</UL>
    </xsl:if>
</xsl:template>

<xsl:template match="tocentry">
    <xsl:choose>
    <xsl:when test="@level = 0">
    <LI><A STYLE="text-align:center; font-size: large;"><xsl:attribute name="href"><xsl:value-of select="@href"/></xsl:attribute><xsl:apply-templates/></A></LI>
    </xsl:when>
    <xsl:when test="@level &lt; 4">
    <LI><A><xsl:attribute name="href"><xsl:value-of select="@href"/></xsl:attribute><xsl:apply-templates/></A></LI>
    </xsl:when>
    </xsl:choose>
</xsl:template>

<xsl:template match="layout[@class='Part']">
    <H1 style="text-align: center" id="{label[position()=1]/@id}">
    <xsl:variable name="part">
	<xsl:number level="any" count="layout[@class='Part']" format="I"/>
    </xsl:variable>
    Part <xsl:value-of select="$part"/><BR/><xsl:apply-templates/>
    </H1>
</xsl:template>

<xsl:template match="layout[@class='Chapter']"><H1 id="{label[position()=1]/@id}"><xsl:apply-templates/></H1></xsl:template>
<xsl:template match="layout[@class='Section']"><H2 id="{label[position()=1]/@id}"><xsl:apply-templates/></H2></xsl:template>
<xsl:template match="layout[@class='Subsection']"><H3 id="{label[position()=1]/@id}"><xsl:apply-templates/></H3></xsl:template>
<xsl:template match="layout[@class='Subsection*']"><H3 id="{label[position()=1]/@id}"><xsl:apply-templates/></H3></xsl:template>
<xsl:template match="layout[@class='Subsubsection']"><H4 id="{label[position()=1]/@id}"><xsl:apply-templates/></H4></xsl:template>
<xsl:template match="layout[@class='Subparagraph']"><H5 id="{label[position()=1]/@id}"><xsl:apply-templates/></H5></xsl:template>
<xsl:template match="layout[@class='LyX-Code']"><PRE id="{label[position()=1]/@id}"><xsl:apply-templates/></PRE></xsl:template>
<xsl:template match="layout[@class='Quote']"><BLOCKQUOTE><P id="{label[position()=1]/@id}"><xsl:apply-templates/></P></BLOCKQUOTE></xsl:template>
<xsl:template match="layout[@class='Quotation']"><BLOCKQUOTE><P id="{label[position()=1]/@id}"><xsl:apply-templates/></P></BLOCKQUOTE></xsl:template>
<xsl:template match="layout[@class='Comment']"/>

<!-- these deserve better handling -->
<xsl:template match="layout[@class='Author']">
<DIV style="font-size: xx-large; text-align: center">
<xsl:apply-templates/>
</DIV>
</xsl:template>

<xsl:template match="layout[@class='Title']">
<DIV style="font-size: xx-large; text-align: center">
<xsl:apply-templates/>
</DIV>
</xsl:template>


<xsl:template match="layout[@class='Standard']">
    <P>
	<xsl:if test="align/@side != ''">
	    <xsl:attribute name="class">a_<xsl:value-of select="align/@side"/>
	    </xsl:attribute>
	</xsl:if>
	<xsl:apply-templates/></P>
</xsl:template>
<xsl:template match="cell/layout[@class='Standard']">
    <xsl:apply-templates/>
</xsl:template>


<!--
<xsl:template match="cell/layout[@class='Standard']">
<DIV STYLE="float:right; color: #ececec; background:black;"><i>cell/layout</i></DIV>
    <xsl:apply-templates/>
</xsl:template>
-->

<xsl:template match="layout">
    <xsl:message>Unrecognized layout class: <xsl:value-of select="@class"/></xsl:message>
    <DIV><xsl:attribute name="class"><xsl:value-of select="@class"/></xsl:attribute><xsl:apply-templates/></DIV>
</xsl:template>

<xsl:template match="newline"><BR/></xsl:template>

<xsl:template match="enumerate"><OL><xsl:apply-templates/></OL></xsl:template>
<xsl:template match="itemize"><UL><xsl:apply-templates/></UL></xsl:template>
<xsl:template match="item"><LI id="{label[position()=1]/@id}"><xsl:apply-templates/></LI></xsl:template>
<xsl:template match="descr"><DL id="{label[position()=1]/@id}"><xsl:apply-templates/></DL></xsl:template>
<xsl:template match="term"><DT id="{label[position()=1]/@id}"><xsl:apply-templates/></DT></xsl:template>
<xsl:template match="desc"><DD><xsl:apply-templates/></DD></xsl:template>

<xsl:template match="tabular">
    <TABLE><xsl:apply-templates/></TABLE></xsl:template>
<xsl:template match="row">
    <TR>
	<xsl:attribute name="class">
	    <xsl:if test="@topline">topline</xsl:if>
	    <xsl:if test="@bottomline"><xsl:text> </xsl:text>bottomline</xsl:if>
	</xsl:attribute>
	<xsl:apply-templates/>
    </TR>
</xsl:template>

<xsl:template match="cell">
    <TD>
	<xsl:attribute name="class">
	    <xsl:if test="@leftline">leftline</xsl:if>
	    <xsl:if test="@rightline"><xsl:text> </xsl:text>rightline</xsl:if>
	    <xsl:if test="@valignment"><xsl:text> </xsl:text>v_<xsl:value-of select="@valignment"/></xsl:if>
	    <xsl:if test="@alignment"><xsl:text> </xsl:text>a_<xsl:value-of select="@alignment"/></xsl:if>
	</xsl:attribute>
	<xsl:apply-templates/>
    </TD>
</xsl:template>

<xsl:template match="font">
    <SPAN>
	<xsl:attribute name="class">
	    <xsl:if test="@family"><xsl:text> f_</xsl:text><xsl:value-of select="@family"/></xsl:if>
	    <xsl:if test="@series"><xsl:text> s_</xsl:text><xsl:value-of select="@series"/></xsl:if>
	    <xsl:if test="@emph"><xsl:text> emph</xsl:text><xsl:value-of select="@emph"/></xsl:if>
	    <xsl:if test="@noun"><xsl:text> noun</xsl:text></xsl:if>
	</xsl:attribute>
	<xsl:apply-templates/>
    </SPAN>
</xsl:template>
<xsl:template match="preamble"/>

<xsl:template match="ref">
    <xsl:choose>
	<xsl:when test="//*[@id=current()/@target]">
	    <A><xsl:attribute name="href">#<xsl:value-of select="@target"/></xsl:attribute>[.]</A>
	</xsl:when>
        <xsl:when test="document('xref.xml')//label[@anchor=current()/@target]">
	    <A><xsl:attribute name="href">
		<xsl:value-of select="document('xref.xml')//label[@anchor=current()/@target]/@src"/>#<xsl:value-of select="@target"/></xsl:attribute>[-&gt;]</A>
	</xsl:when>
	<xsl:otherwise>
            <xsl:message>Unresolved cross-reference <xsl:value-of select="@target"/></xsl:message>
	</xsl:otherwise>
    </xsl:choose>
</xsl:template>
<xsl:template match="label">
    <A><xsl:attribute name="name"><xsl:value-of select="@id"/></xsl:attribute></A>
</xsl:template>

<xsl:template match="graphics">
    <IMG>
	<xsl:attribute name="src"><xsl:value-of select="@src"/></xsl:attribute>
	<xsl:attribute name="width"><xsl:value-of select="@width"/></xsl:attribute>
	<xsl:attribute name="height"><xsl:value-of select="@height"/></xsl:attribute>
    </IMG>
</xsl:template>

<xsl:template match="float">
    <DIV CLASS="float" id="{.//label[position()=1]/@id}">
	<xsl:for-each select="*">
	    <xsl:if test="not(@class='Caption')">
		<xsl:apply-templates/>
	    </xsl:if>
	</xsl:for-each>
	<SPAN CLASS="caption">
	    <xsl:for-each select="*">
		<xsl:if test="@class='Caption'">
		    <xsl:apply-templates/>
		</xsl:if>
	    </xsl:for-each>
	</SPAN>
    </DIV>
</xsl:template>

<xsl:template match="index">
<A><xsl:attribute name="name"><xsl:value-of select="@id"/></xsl:attribute></A>
</xsl:template>

<xsl:template match="index" mode="endlist">
<xsl:choose>
    <xsl:when test="./@term=preceding::index/@term">
	<xsl:text>, </xsl:text>
	<A><xsl:attribute name="href">#<xsl:value-of select="@id"/></xsl:attribute>[.]
	</A>
    </xsl:when>
    <xsl:otherwise>
	<xsl:text disable-output-escaping="yes">&lt;li&gt;</xsl:text>
	<A><xsl:attribute name="href">#<xsl:value-of select="@id"/></xsl:attribute>
	<xsl:value-of select="@term"/></A>
    </xsl:otherwise>
</xsl:choose>
<xsl:if test="not(./@term=following::index/@term)">
    <xsl:text disable-output-escaping="yes">&lt;/li&gt;</xsl:text>
</xsl:if>
</xsl:template>

<xsl:template match="title"/>

<xsl:template match="title" mode="htmltitle">
<xsl:apply-templates/>
</xsl:template>

<xsl:template match="formula">
    <span>
	<xsl:attribute name="class"><xsl:value-of select="@class"/></xsl:attribute>
	<xsl:choose>
	    <xsl:when test="@ref">
		<IMG>
		    <xsl:attribute name="src"><xsl:value-of select="@ref"/>.png</xsl:attribute>
		    <xsl:attribute name="height"><xsl:value-of select="@height"/></xsl:attribute>
		    <xsl:attribute name="width"><xsl:value-of select="@width"/></xsl:attribute>
		</IMG>
	    </xsl:when>
	    <xsl:otherwise>
		<xsl:apply-templates/>
	    </xsl:otherwise>
	</xsl:choose>
    </span>
</xsl:template>

<xsl:template match="htmlurl">
    <a href="{@url}"><xsl:apply-templates/></a>
</xsl:template>

<xsl:template match="inset[@data='Note']"/>

</xsl:stylesheet>
