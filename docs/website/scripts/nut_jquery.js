var NUT =
{
  // UPS table DOM ids
  listID: "#ups_list",
  listBodyID: "#ups_list_body",
  
  fields:
  [
    "support_level",
    "manufacturer",
    "model",
    "comment",
    "driver"
  ],
  
  columns:
  [
    ["manufacturer"],
    ["model","comment"],
    ["driver"]
  ],
  
  tableCache: false,
  
  // UPS filter renderers by data column index
  filterRenderers:
  {
    "support_level": function(value)
    {
      var result = [];
      for(var i = 0; i < value; i++) result.push("*");
      return result.join("");
    },
    "driver": function(value)
    {
      if(value.match(/bcmxcp_usb|blazer_usb|megatec_usb|richcomm_usb|tripplite_usb|usbhid-ups/)) return "USB";
      if(value.match(/snmp-ups|netxml-ups/)) return "Network";
      return "Serial";
    }
  },
  
  /**
   * Returns rendered UPS data according to column index
   * @param {integer} index
   * @param {string} value
   */
  renderFilter: function(index, value)
  {
    var renderer = NUT.filterRenderers[this.fields[index]];
    if(typeof renderer == "function")
      return renderer.call(this, value);
    return value;
  },
  
  supportLevelClasses: ["", "red", "orange", "yellow", "green", "blue"],
  
  /**
   * Initialization method
   */
  init: function()
  {
    if(NUT.isIE6) this.initIE6();
    
    this.initFilters();
    this.sortUPSData(UPSData);
    this.buildUPSList(UPSData);
    this.buildFilters(UPSData);
  },
  /**
   * Initialize filter filters references
   */
  initFilters: function()
  {
    // Display filters fieldset hidden by default for user-agents not using javascript
    $("#filters-set").show();
    
    this.filters =
    {
      "support-level": { index: 0, field: $("#support-level") },
      "manufacturer": { index: 1, field: $("#manufacturer") },
      "model": { index: 2, field: $("#model") },
      "connection": { index: 4, field: $("#connection") }
    }
  },
  
  /**
   * Initialize hacks for Internet Explorer 6
   */
  initIE6: function()
  {
    this.filterPNG();
    this.addIE6Warning();
  },
  
  /**
   * Displays a message inviting the user to upgrade
   */
  addIE6Warning: function()
  {
    $("#header").after(
      "<!--[if lte IE 6]>" +
      "<p style=\"color: red; font-weight: bold;\">Your browser is causing web developers a lot of headaches. Please upgrade." +
      "<![endif]-->"
    );
  },
  
  /**
   * Adds transparency to png images ( /!\ does not work for background images)
   */
  filterPNG: function()
  {
    if(!NUT.isIE6) return;
    
    var imgs = $("img");
    for(var i = 0; i < imgs.length; i++)
    {
      var img = imgs[i];
      if(img.src.match(/\.png$/i))
      {
        $(img).replaceWith(
          "<div style=\"width:" + img.clientWidth + "px;height:" + img.clientHeight + "px;filter:progid:DXImageTransform.Microsoft.AlphaImageLoader(src='" + img.src + "', sizingMethod='crop');\"></div>"
        );
      }
    }
  },
  
  /**
   * Sorts table data by manufacturer and driver
   * @param {Object} data
   */
  sortUPSData: function(data)
  {
    var mI = NUT.fields.indexOf("manufacturer"), dI = NUT.fields.indexOf("driver");
    data.sort(function(a,b)
    {
      var toLower = function(ar)
      {
        var res = ar.slice();
        res.forEach(function(i, index) { if(typeof i == "string") res[index] = i.toLowerCase() });
        return res;
      }
      var c = toLower(a), d = toLower(b);
      return c[mI] == d[mI] ? c[dI] > d[dI] : c[mI] > d[mI];
    });
  },
  /**
   * Builds UPS list from provided data
   * @param {array} data
   */
  buildUPSList: function(data)
  {
    var list = $(this.listBodyID);
    
    // If we're rebuilding the original table, just use the one in cache
    if(data == UPSData && this.tableCache)
    {
      list.html(this.tableCache);
      return;
    }
    
    list.empty();
    
    // Bailout if no data
    if(!data || data.length == 0) return;
    
    // Build rows
    var cellHistory = [], rows = [];
    var rowHistory = data[0][0];
    var classes = ["even", "odd"], manufIndex = this.fields.indexOf("manufacturer"), currentClass = 0;
    data.forEach(function(upsRow, rowIndex)
    {
      if(upsRow[manufIndex] != rowHistory)
      {
        currentClass = Number(!currentClass);
        rowHistory = upsRow[manufIndex];
      } 
      var cells = [];
      // Build cells
      this.columns.forEach(function(column, colIndex)
      {
        var cellContent = [];
        column.forEach(function(field) {cellContent.push(upsRow[this.fields.indexOf(field)])}, this);
        cellContent = cellContent.join("<br />");
        
        // Inspect the last cell on this column and increase row span if the current cell has the same content
        var cH = cellHistory[colIndex];
        if(cH && cH.html == cellContent)
          cH.rowSpan += 1;
        else
        {
          var cell = "";
          if(column.indexOf("driver") != -1)
          {
            cell = {html: cellContent, rowSpan: 1, cls: this.supportLevelClasses[upsRow[this.fields.indexOf("support_level")]]};
          }
          else cell = {html: cellContent, rowSpan: 1, cls: classes[currentClass] }
          
          cells.push(cell);
          cellHistory[colIndex] = cell;
        }
      }, this);
      rows.push(cells);
    }, this);
    
    // Generate actual rows/cells tags
    rows.forEach(function(r, index)
    {
      r.forEach(function(c, index)
      {
        r[index] = "<td class='" + c.cls + "' rowspan='" + c.rowSpan + "'>" + c.html + "</td>";
      });
      rows[index] = "<tr>" + r.join("") + "</tr>";
    });
    list.html(rows.join(""));
    
    if(data == UPSData && !this.tableCache) this.tableCache = list.html();
  },
  /**
   * Initialize filters event listeners
   * @param {Object} data
   */
  buildFilters: function(data)
  {
    for(var f in NUT.filters)
    {
      var filter = NUT.filters[f];
      this.populateCombo(data, filter);
      filter.field.change(this.doFilter);
      var op = $("#op-" + (filter.index));
      if(op) op.change(this.doFilter);
    }
  },
  /**
   * Load data in filter combos
   * @param {array} data
   * @param {object} combo
   * @param {integer} index
   */
  populateCombo: function(data, filter)
  {
    var values = [];
    var combo = filter.field;
    data.forEach(function(row)
    {
      var value = NUT.renderFilter(filter.index, row[filter.index]);
      if(value != "" && values.indexOf(value) < 0) values.push(value);
    }, this);
    values = values.sort();
    var oldValue = combo.val();
    combo.html("<option value='-1'>---</option>");
    values.forEach(function(value)
    {
      var option = $(document.createElement("option"));
      option.val(value);
      option.text(value);
      if(filter.field.id == "support_level");
        option.attr("class", this.supportLevelClasses[value]);
      combo.append(option);
    }, this);
    combo.val(oldValue);
  },
  /**
   * Apply selected filters on UPS list
   */
  doFilter: function()
  {
    var initialRows = UPSData.slice();
    var filteredRows = UPSData.slice();
    
    /**
     * Applies a single filter on provided UPS data set
     * @param {string} value
     * @param {integer} index
     * @param {string} operator
     * @param {array} data
     * @return {array} filtered data set
     */
    var applyFilter = function(value, index, data)
    {
      var tmpData = [];
      tmpData = data.slice();
      tmpData.forEach(function(row, rowIndex)
      {
        var field = row[index];
        if(NUT.renderFilter(index, field) != value) data.splice(data.indexOf(row), 1);
      }, this);
      return data;
    }
    
    // Sequentially apply filters
    for(var f in NUT.filters)
    {
      var filter = NUT.filters[f];
      var value = filter.field.val();
      if(value != "-1") // Is filter active, i.e have the user picked a value in the filter combo
      {
        var opField = $("#op-" + filter.index);
        filteredRows = applyFilter(value, filter.index, filteredRows);
      }
    }
    
    // Rebuild UPS list and combos according to filtered data
    NUT.buildUPSList(filteredRows);
    ["manufacturer", "model", "connection"].forEach(function(id)
    {
      if(this.id != id) NUT.populateCombo(filteredRows, NUT.filters[id]);
    }, this);
  },
  
  resetCombos: function()
  {
    for(var f in NUT.filters)
    {
      var field = NUT.filters[f].field;
      this.populateCombo(UPSData, NUT.filters[f]);
      field.val("-1");
    }
    NUT.buildUPSList(UPSData);
  }
}

if(typeof Array.prototype.indexOf != "function")
{
  Array.prototype.indexOf = function(elt)
  {
    var i = 0;
    while(i < this.length)
    {
      if(this[i] == elt) return i;
      i++;
    }
    return -1;
  }
}
if(typeof Array.prototype.forEach != "function")
{
  Array.prototype.forEach = function(cb, scope)
  {
    var i = 0;
    while(i < this.length)
    {
      cb.call(scope || this, this[i], i, this);
      i++;
    }
  }
}

// Global initialization
$(function()
{
  var ua = navigator.userAgent.toLowerCase();
  NUT.isIE6 = !/opera/.test(ua) && /msie/.test(ua) && !/msie 7/.test(ua) && !/msie 8/.test(ua);
  NUT.init.call(NUT);
});
