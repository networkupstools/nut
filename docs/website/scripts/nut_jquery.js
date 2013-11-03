var NUT =
{
  // UPS table DOM ids
  listID: "#ups_list",
  listBodyID: "#ups_list_body",
  
  // Field names
  fields:
  [
    "manufacturer",
    "device-type",
    "support-level",
    "model",
    "comment",
    "driver"
  ],
  
  // Actual HTML table columns
  columns:
  [
    ["manufacturer"],
    ["model","comment"],
    ["driver"],
    ["support-level"],
  ],
  
  // driver => connection type mappings
  driverMap: function(driver)
  {
    if(driver.match(/bcmxcp_usb|blazer_usb|richcomm_usb|tripplite_usb|usbhid-ups/))
      return "USB";
      
    if(driver.match(/snmp-ups|netxml-ups/))
      return "Network";
     
    return "Serial";
  },
  
  // Support level => CSS class mappings
  supportLevelClasses:
  {
    0: "",
    1: "red",
    2: "orange",
    3: "yellow",
    4: "blue",
    5: "green"
  },
  
  tableCache: false,

  // Parse GET parameters from window url and return them as a hash
  // The call format is:
  // stable-hcl.html?<filter name>=<filter value>
  // Refer to docs/website/stable-hcl.txt for examples
  parseGetParameters: function()
  {
    var url = window.location.href;
    url = url.replace(/#$/, "");
    var fieldPos = url.indexOf("?");
    var get = {};
    if(fieldPos > -1)
    {
      var fileName = url.substring(0, fieldPos);
      var getList = url.substring(fieldPos + 1).split("&");
      for(var i = 0; i<getList.length; i++)
      {
        var getField = getList[i].split("=");
        get[unescape(getField[0])] = unescape(getField[1]);
      }
    }
    return get;
  },
 
  // UPS filter renderers by data column index
  filterRenderers:
  {
    "support-level": function(value)
    {
      var result = [];
      for(var i = 0; i < value; i++) result.push("*");
      return result.join("");
    },
    "driver": function(value)
    {
      return this.driverMap(value);
    },
    "device-type": function(value)
    {
      var map = 
      {
        "pdu": "Power Distribution Unit",
        "ups": "Uninterruptible Power Supply",
        "scd": "Solar Controller Device"
      }
      
      return map[value];
    }
  },
  
  // Specific filter handlers
  filterHandlers:
  {
    /**
     * @param {string} value value to filter
     * @param {array} row raw data fields
     * @return {boolean} true if value passes the filter, false otherwise
     */
    "driver": function(value, row)
    {
      var driver = row[this.fields.indexOf("driver")];
      if(this.driverMap(driver) == value) return true;
      
      return false;
    }
  },
  
  /**
   * Returns rendered UPS data according to column index
   * @param {integer} index
   * @param {string} value
   */
  renderFilter: function(index, value)
  {
    var renderer = this.filterRenderers[this.fields[index]];
    if(typeof renderer == "function")
      return renderer.call(this, value);
    return value;
  },
  
  /**
   * Initialization method
   */
  init: function()
  {
    this.initFilters();
    this.sortUPSData(UPSData);
    this.buildUPSList(UPSData);
    this.buildFilters(UPSData);
    
    var get = this.parseGetParameters();
    for(var param in get)
    {
      var filter = $("#"+param);
      if(filter)
      {
        filter.val(get[param]);
        this.doFilter();
      }
    }
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
      "support-level": { index: this.fields.indexOf("support-level"), field: $("#support-level") },
      "device-type": { index: this.fields.indexOf("device-type"), field: $("#device-type") },
      "manufacturer": { index: this.fields.indexOf("manufacturer"), field: $("#manufacturer") },
      "model": { index: this.fields.indexOf("model"), field: $("#model") },
      "driver": { index: this.fields.indexOf("driver"), field: $("#connection") }
    }
  },
  
  /**
   * Sorts table data by manufacturer and driver
   * @param {Object} data
   */
  sortUPSData: function(data)
  {
    var mI = this.fields.indexOf("manufacturer"), dI = this.fields.indexOf("driver");
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
    
    // Initialize table cache
    if(!this.tableCache) this.tableCache = list.html();
    
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
        if(column.indexOf("driver") == -1 && cH && cH.html == cellContent)
          cH.rowSpan += 1;
        else
        {
          var cell = "";
          if(column.indexOf("driver") != -1)
          {
            cell = {html: cellContent, rowSpan: 1, cls: this.supportLevelClasses[upsRow[this.fields.indexOf("support-level") || ""]]};
          }
          else cell = {html: cellContent, rowSpan: 1, cls: classes[currentClass] }
          
          if(column.indexOf("support-level") != -1) cell.cls += " hidden";
          
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
        r[index] = ["<td class='", c.cls, "' rowspan='", c.rowSpan, "'>", c.html, "</td>"].join("");
      });
      rows[index] = ["<tr>", r.join(""), "</tr>"].join("");
    });
    
    list.html(rows.join(""));
  },
  /**
   * Initialize filters event listeners
   * @param {Object} data
   */
  buildFilters: function(data)
  {
    for(var f in this.filters)
    {
      var filter = this.filters[f];
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
    var valueDict = {};
    
    var combo = filter.field;
    var oldValue = combo.val();
    combo.html("<option value='-1'>---</option>");
    
    // Special case for connection type
    if(filter.field.attr("id") == "connection")
    {
      ["Serial", "USB", "Network"].forEach(function(type)
      {
        values.push([type, type]);
      }, this);
    }
    else
    {
      data.forEach(function(row)
      {
        var value = row[filter.index];
        if(value != "" && !valueDict[value])
        {
          values.push([value, this.renderFilter(filter.index, value)]);
          valueDict[value] = true;
        }
      }, this);
      
      values = values.sort();
    }
    
    values.forEach(function(value)
    {
      var option = $(document.createElement("option"));
      option.val(value[0]);
      option.text(value[1]);
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
     * @param {array} data
     * @returns {array} filtered data set
     */
    var applyFilter = function(value, index, data)
    {
      var tmpData = [];
      tmpData = data.slice();
      tmpData.forEach(function(row, rowIndex)
      {
        var field = row[index];
        var handler = this.filterHandlers[this.fields[index]];
        if(handler)
        {
          if(!handler.apply(this, [value, row]))
          {
            data.splice(data.indexOf(row), 1);
          }
        }
        else if(row[index] != value) data.splice(data.indexOf(row), 1);
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
        filteredRows = applyFilter.apply(NUT, [value, filter.index, filteredRows]);
      }
    }
    
    // Rebuild UPS list and combos according to filtered data
    NUT.buildUPSList(filteredRows);
    ["manufacturer", "model", "driver"].forEach(function(id)
    {
      if(this.id != id) this.populateCombo(filteredRows, this.filters[id]);
    }, NUT);
  },
  
  resetCombos: function()
  {
    for(var f in this.filters)
    {
      var field = this.filters[f].field;
      this.populateCombo(UPSData, this.filters[f]);
      field.val("-1");
    }
    this.buildUPSList(UPSData);
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
    for (var i = 0, n = this.length; i<n; i++)
      if (i in this)
        cb.call(scope, this[i], i, this);
  }
}

// Global initialization
$(function()
{
  NUT.init.call(NUT);
});
