<?xml version="1.0" encoding="UTF-8"?>
<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">

	<xsl:template match="/"> 
		 <xsl:apply-templates select="//CCC"/> 
	</xsl:template>
	<xsl:template match="CCC"> 
		 <h3 style="color:blue">
			  <xsl:value-of select="name()"/> 
			  <xsl:text> id=</xsl:text> 
			  <xsl:value-of select="@id"/> 
		 </h3> 
	</xsl:template>
	<xsl:template match="CCC/CCC"> 
		 <h2 style="color:red">
			  <xsl:value-of select="name()"/> 
			  <xsl:text> id=</xsl:text> 
			  <xsl:value-of select="@id"/> 
		 </h2> 
	</xsl:template>


</xsl:stylesheet>

