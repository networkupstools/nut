$(function()
{
  var resizeContent = function()
  {
    $("#layout-content-box").css("height", $(window).height() - $("#layout-banner-box").height());
  }
  $(window).resize(resizeContent);
  resizeContent();
});
