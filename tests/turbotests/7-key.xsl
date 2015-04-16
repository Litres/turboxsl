<?xml version="1.0" encoding="UTF-8"?>
<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform" xmlns:fo="http://www.w3.org/1999/XSL/Format" xmlns:chk="LTR:chk" xmlns:ltr="LTR" exclude-result-prefixes="ltr chk" xmlns:exsl="http://exslt.org/common" extension-element-prefixes="exsl">

<xsl:key name="title-search" match="book" use="@author"/>
<xsl:template match="/">
	<head>
		<meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
	</head>
	<body>
		<xsl:for-each select="key('title-search', 'David Perry')">
				 <xsl:value-of select="@title"/><br/>
      </xsl:for-each>
	</body>
</xsl:template>

</xsl:stylesheet>
