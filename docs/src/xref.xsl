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
    encoding="UTF-8"/>

<xsl:param name="docname"/>

<xsl:template match="/">
    <HTML>
    <HEAD>
    <TITLE>Cross-reference Index</TITLE>
    <STYLE TYPE="text/css">
h1, h2 { background: #c0c0f0; }
h1, h2, h3, h4, h5 { border-bottom: 2px solid #8080c0; color: black; }

div.nav { float: right; background: #ffffff; }

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
    <DIV class="nav">
        <xsl:if test="document('docs.xml')//doc[@name=$docname]/preceding-sibling::*[position()=1]">
            <A HREF="{document('docs.xml')//doc[@name=$docname]/preceding-sibling::*[position()=1]/@name}.html" TITLE="Previous: {document('docs.xml')//doc[@name=$docname]/preceding-sibling::*[position()=1]/@title}">[&lt;-]</A><xsl:text> </xsl:text>
        </xsl:if>
        <A HREF="index.html" TITLE="Up: Documentation Index" >[^]</A><xsl:text> </xsl:text>
        <xsl:if test="document('docs.xml')//doc[@name=$docname]/following-sibling::*[position()=1]">
            <A HREF="{document('docs.xml')//doc[@name=$docname]/following-sibling::*[position()=1]/@name}.html" TITLE="Next: {document('docs.xml')//doc[@name=$docname]/following-sibling::*[position()=1]/@title}">[-&gt;]</A>
        </xsl:if>
    </DIV>
    <H1>Index</H1>
    <UL style="-moz-column-width: 20ex; -moz-column-gap: 4ex;">
    <!-- <index anchor="Parport" src="drivers.html"/> -->
    <xsl:for-each select="//index">
	<xsl:sort select="@lcterm"/>
	<xsl:sort select="@src"/>
	<xsl:sort select="@href"/>
	<xsl:choose>
	    <xsl:when test="./@term=preceding::index/@term">
		<xsl:text>, </xsl:text>
		<A><xsl:attribute name="href"><xsl:value-of select="@src"/>#<xsl:value-of select="@anchor"/></xsl:attribute>[-]</A>
	    </xsl:when>
	    <xsl:otherwise>
		<xsl:text disable-output-escaping="yes">&lt;li&gt;</xsl:text>
		<A><xsl:attribute name="href"><xsl:value-of select="@src"/>#<xsl:value-of select="@anchor"/></xsl:attribute>
		<xsl:value-of select="@term"/></A>
	    </xsl:otherwise>
	</xsl:choose>
    </xsl:for-each>
    </UL>
    </BODY>
    </HTML>
</xsl:template>

</xsl:stylesheet>
