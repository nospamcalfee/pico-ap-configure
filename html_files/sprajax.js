function create_checkboxes(divinner, sched, disablech=0) {
    var dspan = document.createElement("span");
    dspan.setAttribute('class', 'w3-tiny');
    dspan.setAttribute('style', 'white-space: nowrap;');
    var weekdays = ["Sunday", "Monday", "Tuesday", "Wednesday","Thursday","Friday","Saturday"];
    for (var l = 0; l < weekdays.length; l++) {
        var tspan = dspan.cloneNode(true);
        divinner.appendChild(tspan);
        var tinput = document.createElement("input");
        tinput.setAttribute('type', 'checkbox');
        tinput.setAttribute('name', weekdays[l]);
        var tlabel = document.createElement("label");
        if (sched['days'].hasOwnProperty(weekdays[l])) {
            tinput.setAttribute('checked', 'true');
        }
        if (disablech) {
            tinput.disabled = true;
        }
        tlabel.setAttribute('class', 'checkbox');
        tlabel.setAttribute('for', weekdays[l]);
        var t = document.createTextNode(weekdays[l].substr(0,3));
        tlabel.appendChild(t);
        tspan.appendChild(tinput);
        tspan.appendChild(tlabel);
        tspan.appendChild(document.createTextNode("\n"));
    }
}
function get_grp_list(schedule) {
    var grp_list = JSON.parse(JSON.stringify(schedule));
    grp_list.sort(function(a,b) { return a.groupname - b.groupname; } );
    // delete all duplicates from the array
    for( var i=0; i<grp_list.length-1; i++ ) {
      if ( grp_list[i].groupname == grp_list[i+1].groupname ) {
        grp_list.splice(i, 1);
        i--;
      }
    }
    return grp_list;
}

function render_valve_control(mirror, locald) {
    var everything = JSON.parse(mirror);
    var localdata = JSON.parse(locald);
    var vi = localdata.valveindex;
    var update = localdata.update=="1";
    var schedule = everything.schedule[vi];
    var sched = schedule['sched'];
    var br = document.createElement("br");
    var eol = document.createTextNode("\n");
    document.title = localdata.title;
    //document.getElementsByTagName('head')[0].appendChild(localdata.hdrtext);
    document.getElementById("serv_valve").innerHTML = localdata.server_ip  +
    " " + localdata.valvename;
    document.getElementById("run_it").style.display = (update) ? "block" : "none";
    document.getElementById("img_id").alt = localdata.valvename;
    if ("jpegname" in schedule) {
        document.getElementById("img_id").src = "/" + schedule["jpegname"];
        if (update) {
            document.getElementById("picdiv1").innerHTML = "Current picture: " + schedule["jpegname"];
        }
    }
    if (update) {
        document.getElementById("picdiv2").style.display = "block"
        document.getElementById("picdiv3").style.display = "none"
    }
    var imgs = localdata.pictures.split(",").sort();
    imgs.push("No Picture")
    var jpegname = document.getElementById("jpegname");
    for (i = 0; i < imgs.length; i++) {
        var opt = document.createElement("option");
        var img = imgs[i].trim();
        opt.value = img;
        opt.text = img;
        if (img == schedule["jpegname"]) {
            opt.selected = "true";
        }
        jpegname.appendChild(opt);
    }
    var buddies = everything.buddy_ip;
    var budname = document.getElementById("budname");
    for (i = 0; i < buddies.length; i++) {
        var opt = document.createElement("option");
        var bud = buddies[i].trim();
        opt.value = bud;
        opt.text = bud;
        if (bud == schedule["budname"]) {
            opt.selected = "true";
        }
        budname.appendChild(opt);
    }
    var gpio = document.getElementById("number");
    gpio.value = (update && "gpio" in schedule) ? schedule["gpio"] : 1;

    var grp_list = get_grp_list(everything.schedule);
    var gn = document.getElementById("lgroupname");
    gn.value = schedule["groupname"];
    var groups = document.getElementById("listgroupname");
    var grplist = ""
    for (i = 0; i < grp_list.length; i++) {
        var opt = document.createElement("option");
        var g = grp_list[i]["groupname"].trim();
        opt.value = g;
        opt.text = g;
        grplist += "'" + g + "' ";
        groups.appendChild(opt);
    }
    document.getElementById("grplist").textContent += grplist;
    document.getElementById("del_last").style.display = (update) ? "block" : "none";
    document.getElementById("update_show2").style.display = (!update) ? "block" : "none";
    document.getElementById("update_show1").style.display = (update) ? "block" : "none";

    document.getElementById("img_show").style.display = (update) ? "block" : "none";
    document.getElementById("valvename").value = schedule['valvename'];
    document.getElementById("valvenamediv").style.display = "block";
    document.getElementById("minutes").value = sched[0]['mins'];
    for (var k = 0; k < sched.length; k++) {
        var fields = document.createElement("fieldset");
        fields.setAttribute('class', 'checkgrid');
        var legend = document.createElement("legend");
        var t = document.createTextNode("Valve Schedule");
        legend.appendChild(t);
        fields.appendChild(legend);
        t = document.createTextNode("Select run days");
        fields.appendChild(t);
        fields.appendChild(br.cloneNode(true));
        fields.appendChild(eol.cloneNode(true));
        var autorunmins = document.getElementById("autorunmins").cloneNode(true);
        autorunmins.id = "autorunmins" + k;
        fields.appendChild(autorunmins);
        autorunmins.style.display="block";
        autorunmins.value = sched[k]["mins"];
        fields.appendChild(eol.cloneNode(true));
        create_checkboxes(fields, sched[k], localdata.update != "1")
        document.getElementById("valvenamediv").appendChild(fields)
    }
};

function render_config(mirror, locald) {
    var everything = JSON.parse(mirror);
    var localdata = JSON.parse(locald);
    document.getElementById("iostatus").innerHTML = "iostatus: " + localdata.io_status;
    document.getElementById("sysname").innerHTML = localdata.sysname  +
    " Sprinkler Server V" + everything.server_version;
    document.getElementById("well_delay").value = everything.well_delay;
    document.getElementById("server_ip").value = everything.server_ip;
    document.getElementById("buddy_ip").value = everything.buddy_ip;
    var buds = "";
    for (var j = 0; j < everything.buddy_ip.length; j++) {
        if (j !== 0) {
            buds += ", "
        }
        buds += everything.buddy_ip[j];
    }
    document.getElementById("buddy_ip").value = buds;
};
function rendermain(jsdata){
    var tableInfo = JSON.parse(jsdata);
    var divvalves = document.createElement("div");
    divvalves.setAttribute('class', 'w3-container');
    var lname = "asdfj@@#!";
    var schedule = tableInfo.schedule;
    var grp_list = get_grp_list(schedule);
    var eol = document.createTextNode("\n");
    var br = document.createElement("br");
    for (var i = 0; i < grp_list.length; i++) {
        var gname = grp_list[i]['groupname'];
        for (var j = 0; j < schedule.length; j++) {
            if (gname == schedule[j]['groupname']) {
                if (gname != lname) {
                    lname = gname;
                    var para = document.createElement("p"); //<p>
                    para.style = 'clear:left;';
                    divvalves.appendChild(para);
                    divvalves.appendChild(eol.cloneNode(true));
                }
                var anchor = document.createElement("a")
                anchor.href = "controlvalve.html/?valveindex=" + j;
                anchor.setAttribute('class','mycolumns');
                var gn = (!lname) ? '""' : lname;
                var t = document.createTextNode("\nGroup: " + gn);
                anchor.appendChild(t);
                divvalves.appendChild(eol.cloneNode(true));
                divvalves.appendChild(anchor);
                var divouter = document.createElement("div");
                divouter.setAttribute('class', 'w3-container w3-border w3-green w3-text-black');
                divouter.appendChild(eol.cloneNode(true));
                anchor.appendChild(divouter)
                  //fixme <div type="submit"  class="w3-container  w3-border w3-green w3-text-black">
                var img1 = document.createElement("img");
                img1.setAttribute('class', "w3-image");
                img1.src = schedule[j]['jpegname'];
                img1.setAttribute('onerror', "this.src='watering-can---game-component---superb-quality-144-203249.png';this.onerror='';");
                img1.alt = "no picture available" + "\n"
                divouter.appendChild(eol.cloneNode(true));
                divouter.appendChild(img1);
                var divinner = document.createElement("div");
                divinner.setAttribute('class', 'w3-panel w3-border');
                var t = document.createTextNode(schedule[j]['budname'] + " ");
                divinner.appendChild(t);
                var divt = document.createElement("div");
                divt.setAttribute('class', 'sprinkler-name');
                divinner.appendChild(divt);
                var t = document.createTextNode(schedule[j]['valvename'] + "\n");
                divinner.appendChild(t);
                divouter.appendChild(divinner);
                var sched = schedule[j]['sched'];
                for (var k = 0; k < sched.length; k++) {
                    var t = document.createTextNode("\nStart at " + sched[k]['time'] + " for " + sched[k]['mins'] + " minutes\n");
                    divinner.appendChild(br.cloneNode(true));
                    divinner.appendChild(t);
                    divinner.appendChild(br.cloneNode(true));
                    create_checkboxes(divinner, sched[k]);
                   /* var dspan = document.createElement("span");
                    dspan.setAttribute('class', 'w3-tiny');
                    dspan.setAttribute('style', 'white-space: nowrap;');
                    var weekdays = ["Sunday", "Monday", "Tuesday", "Wednesday","Thursday","Friday","Saturday"];
                    for (var l = 0; l < weekdays.length; l++) {
                        var tspan = dspan.cloneNode(true);
                        divinner.appendChild(tspan);
                        var tinput = document.createElement("input");
                        tinput.setAttribute('type', 'checkbox');
                        tinput.setAttribute('name', weekdays[l]);
                        var tlabel = document.createElement("label");
                        if (sched[k]['days'].hasOwnProperty(weekdays[l])) {
                            tinput.setAttribute('checked', 'true');
                        }
                        tinput.disabled = true;
                        tlabel.setAttribute('class', 'checkbox');
                        tlabel.setAttribute('for', weekdays[l]);
                        var t = document.createTextNode(weekdays[l].substr(0,3));
                        tlabel.appendChild(t);
                        tspan.appendChild(tinput);
                        tspan.appendChild(tlabel);
                        tspan.appendChild(eol.cloneNode(true));
                    }*/
                }
            }
        }
    }
    //</div>
    document.body.appendChild(divvalves);
};

function getTableDataAJAX(url, callback, args="") {
    var getTableData = new XMLHttpRequest();

    getTableData.onreadystatechange = function() {
        //checking for when response is received from server
        if(getTableData.readyState === XMLHttpRequest.DONE) {
          if (getTableData.status === 200) {
            //alert(getTableData.responseText);
            callback.call(getTableData.responseText);
            } else {
            alert("error fetching json");
          }
        };
    };
    getTableData.open("GET", url, true);
    getTableData.send();
};

function jsonalert(inxxx) {
    var j = JSON.parse(inxxx);
    alert("jsonalert " + JSON.stringify(j));
};
