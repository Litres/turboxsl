<?xml version="1.0" encoding="UTF-8"?>
<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform" xmlns:fo="http://www.w3.org/1999/XSL/Format" xmlns:ltr="LTR">
<xsl:template match="test">
	<head>
		<meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
	</head>
	<body>
		<ol>
		<li><xsl:value-of select="@username"/></li>
		<li><xsl:value-of select="@fullname"/></li>
		<li><xsl:value-of select="@username | @fullname"/></li>
		<li><xsl:value-of select="@fullname | @username"/></li>
		<li><xsl:value-of select="sub_test/@username | @fullname"/></li>
		<li><xsl:value-of select="@username | sub_test/@fullname"/></li>
		<li><xsl:value-of select="sub_test/@username | sub_test/@fullname"/></li>
		<li><xsl:value-of select="sub_test/@fullname | sub_test/@username"/></li>
		<li><xsl:value-of select="sub_test/@username | sub_test2/@username"/></li>
		<li><xsl:value-of select="sub_test2/@fullname|sub_test/@fullname|sub_test/../@fullname"/></li>
		<li><xsl:value-of select="sub_test2/@fullname|sub_test/@fullname"/></li>
		</ol>
		<xsl:apply-templates/>
	</body>
</xsl:template>

</xsl:stylesheet>
