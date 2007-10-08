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
    xmlns:fo="http://www.w3.org/1999/XSL/Format"
    version="1.0">
<xsl:output method="html"/>

<xsl:template match="/">
    <HTML>
    <HEAD>
    <TITLE>
	<xsl:apply-templates select="//title" mode="htmltitle"/>
    </TITLE>
    <STYLE TYPE="text/css">
h1, h2 { background: #c0c0f0; }
h1, h2, h3, h4, h5 { border-bottom: 2px solid #8080c0; color: black; }

dt { font-weight: bold; }
pre { margin-left: 4ex; auto; color: black; padding: 1ex; border-left: 2px solid #8080c0; }
div.float { text-align: center; margin: 2ex; }
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

    </STYLE>
    </HEAD>
    <BODY>
    <xsl:apply-templates/>
    <xsl:if test="//footnote">
	<H3>Footnotes</H3>
	<xsl:apply-templates select="//footnote" mode="endlist"/>
    </xsl:if>
    <xsl:if test="//index">
	<H3>Index</H3>
	<UL style="-moz-column-width: 20ex; -moz-column-gap: 4ex">
	<xsl:for-each select="//index">
	    <xsl:sort select="@term"/>
	    <xsl:apply-templates select="." mode="endlist"/>
	</xsl:for-each>
	</UL>
    </xsl:if>
    </BODY>
    </HTML>
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
	<UL style="-moz-column-width: 40ex; -moz-column-gap: 4ex;">
	<xsl:apply-templates/>
	</UL>
    </xsl:if>
</xsl:template>

<xsl:template match="tocentry">
    <xsl:if test="@level &lt; 4">
    <LI><A><xsl:attribute name="href"><xsl:value-of select="@href"/></xsl:attribute><xsl:apply-templates/></A></LI>
    </xsl:if>
</xsl:template>

<xsl:template match="layout[@class='Chapter']"><H1><xsl:apply-templates/></H1></xsl:template>
<xsl:template match="layout[@class='Section']"><H2><xsl:apply-templates/></H2></xsl:template>
<xsl:template match="layout[@class='Subsection']"><H3><xsl:apply-templates/></H3></xsl:template>
<xsl:template match="layout[@class='Subsection*']"><H3><xsl:apply-templates/></H3></xsl:template>
<xsl:template match="layout[@class='Subsubsection']"><H4><xsl:apply-templates/></H4></xsl:template>
<xsl:template match="layout[@class='Subparagraph']"><H5><xsl:apply-templates/></H5></xsl:template>
<xsl:template match="layout[@class='LyX-Code']"><PRE><xsl:apply-templates/></PRE></xsl:template>
<xsl:template match="layout[@class='Quote']"><BLOCKQUOTE><P><xsl:apply-templates/></P></BLOCKQUOTE></xsl:template>
<xsl:template match="layout[@class='Quotation']"><BLOCKQUOTE><P><xsl:apply-templates/></P></BLOCKQUOTE></xsl:template>
<xsl:template match="layout[@class='Comment']"/>

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
<xsl:template match="item"><LI><xsl:apply-templates/></LI></xsl:template>
<xsl:template match="descr"><DL><xsl:apply-templates/></DL></xsl:template>
<xsl:template match="term"><DT><xsl:apply-templates/></DT></xsl:template>
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
	<xsl:otherwise>
	    <A><xsl:attribute name="href">
		<xsl:value-of select="document('xref.xml')//label[@anchor=current()/@target]/@src"/>#<xsl:value-of select="@target"/></xsl:attribute>[-&gt;]</A>
	</xsl:otherwise>
    </xsl:choose>
</xsl:template>
<xsl:template match="label">
    <A><xsl:attribute name="id"><xsl:value-of select="@id"/></xsl:attribute></A>
</xsl:template>

<xsl:template match="graphics">
    <IMG>
	<xsl:attribute name="src"><xsl:value-of select="@src"/></xsl:attribute>
	<xsl:attribute name="width"><xsl:value-of select="@width"/></xsl:attribute>
	<xsl:attribute name="height"><xsl:value-of select="@height"/></xsl:attribute>
    </IMG>
</xsl:template>

<xsl:template match="float">
    <DIV CLASS="float">
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
<A><xsl:attribute name="id"><xsl:value-of select="@id"/></xsl:attribute></A>
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
		    <xsl:attribute name="height"><xsl:value-of select="@height"/>.png</xsl:attribute>
		    <xsl:attribute name="width"><xsl:value-of select="@width"/>.png</xsl:attribute>
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
