<?xml version="1.0"?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform" version="1.0">

  <!-- Keep a2x's AsciiDoc style and add the NUT presentation layer. -->
  <xsl:param name="monoseq.small">1</xsl:param>
  <xsl:template match="book|article" mode="docinfo">
    <xsl:apply-imports/>
    <xsl:text>\usepackage{nut-pdf}&#10;</xsl:text>
  </xsl:template>

  <!-- AsciiDoc emits inline commands, paths and identifiers as literals.
       Explicit break points allow wrapping; a drawn continuation mark keeps
       wrapped tokens distinct without changing their copied text.
       Keep nested markup on dblatex's normal rendering path. -->
  <xsl:template match="literal[not(*)]|code[not(*)]">
    <xsl:text>\NUTliteral{</xsl:text>
    <xsl:call-template name="scape">
      <xsl:with-param name="string" select="string(.)"/>
    </xsl:call-template>
    <xsl:text>}{\texttt{\small{</xsl:text>
    <xsl:call-template name="nut.literal.text">
      <xsl:with-param name="text" select="string(.)"/>
    </xsl:call-template>
    <xsl:text>}}}</xsl:text>
  </xsl:template>


  <!-- Keep hyperlink destinations unchanged while allowing displayed URLs and
       long variable-list terms to wrap. -->
  <xsl:template match="ulink[not(*) and string(.) = @url]/text()|term/emphasis[not(*)]/text()">
    <xsl:text>\NUTliteral{</xsl:text>
    <xsl:call-template name="scape">
      <xsl:with-param name="string" select="string(.)"/>
    </xsl:call-template>
    <xsl:text>}{</xsl:text>
    <xsl:call-template name="nut.literal.text">
      <xsl:with-param name="text" select="string(.)"/>
    </xsl:call-template>
    <xsl:text>}</xsl:text>
  </xsl:template>

  <xsl:template name="nut.literal.text">
    <xsl:param name="text"/>
    <xsl:if test="string-length($text)">
      <xsl:variable name="char" select="substring($text, 1, 1)"/>
      <xsl:choose>
        <xsl:when test="$char = ' '"><xsl:text>\NUTliteralspace{}</xsl:text></xsl:when>
        <xsl:otherwise>
          <xsl:text>\hbox{</xsl:text>
          <xsl:call-template name="scape">
            <xsl:with-param name="string" select="$char"/>
          </xsl:call-template>
          <xsl:text>}</xsl:text>
          <xsl:if test="string-length($text) &gt; 1 and substring($text, 2, 1) != ' '">
            <xsl:text>\NUTliteralbreak{}</xsl:text>
          </xsl:if>
        </xsl:otherwise>
      </xsl:choose>
      <xsl:call-template name="nut.literal.text">
        <xsl:with-param name="text" select="substring($text, 2)"/>
      </xsl:call-template>
    </xsl:if>
  </xsl:template>
</xsl:stylesheet>
