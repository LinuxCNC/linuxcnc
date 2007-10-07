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
    <TITLE>Cross-reference Index</TITLE>
    </HEAD>

    <BODY>
    <UL style="-moz-column-count: 4">
    <!-- <index anchor="Parport" src="drivers.html"/> -->
    <xsl:for-each select="//index">
	<xsl:sort select="@term"/>
	<xsl:sort select="@src"/>
	<xsl:sort select="@href"/>
	<xsl:choose>
	    <xsl:when test="./@term=preceding::index/@term">
		<xsl:text>, </xsl:text>
		<A><xsl:attribute name="href"><xsl:value-of select="@src"/>#<xsl:value-of select="@anchor"/></xsl:attribute>
[->]
		</A>
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
