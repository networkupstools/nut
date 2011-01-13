$(function()
{
  var imgs = $("img");
  for(var i = 0; i < imgs.length; i++)
  {
    var img = $(imgs[i]);
    if(img.attr("src").match(/\.png$/i))
    {
      img.replaceWith(
        "<div style=\"width:" + img.width() + "px;height:" + img.height() + "px;filter:progid:DXImageTransform.Microsoft.AlphaImageLoader(src='" + img.attr("src") + "', sizingMethod='crop');\"></div>"
      );
    }
  }
});