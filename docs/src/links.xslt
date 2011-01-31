<?xml version="1.0" encoding="utf-8"?>
<!--
  vim: sts=2 sw=2 et
  -->
<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
<xsl:output method='text'/>

<xsl:template match="/">
  <xsl:for-each select='//anchor'>
    <xsl:value-of select='@id'/><xsl:text>
</xsl:text>
  </xsl:for-each>
</xsl:template>

</xsl:stylesheet>
