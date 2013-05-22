/*!
 * miniemc2 javascript applications
 * http://miniemc2.googlecode.com
 *
 * Copyright 2012, Sergey Kaydalov, kayserg@gmail.com
 * Licensed under the GPL Version 2 licenses.
 */
function miniemc2Init() {
    miniemc2Init.filelist_inited = false;
    InitList("#axes_list", DispNextAxisName, DispPrevAxisName);
    InitList("#movement_type_list", NextMovementQauntity, PrevMovementQauntity);
    InitList("#offlist", NextFixture, PrevFixture);
    InitPicButton("#jright", function () {
        IssueCommand("jog", "X+" + GetMovementQuantity());
        if (GetMovementQuantity() == 0) PeriodicJogRepeat("X+" + GetMovementQuantity());
    });
    InitPicButton("#jleft", function () {
        IssueCommand("jog", "X-" + GetMovementQuantity());
        if (GetMovementQuantity() == 0) PeriodicJogRepeat("X-" + GetMovementQuantity());
    });
    InitPicButton("#jup", function () {
        IssueCommand("jog", "Y+" + GetMovementQuantity());
        if (GetMovementQuantity() == 0) PeriodicJogRepeat("Y+" + GetMovementQuantity());
    });
    InitPicButton("#jdown", function () {
        IssueCommand("jog", "Y-" + GetMovementQuantity());
        if (GetMovementQuantity() == 0) PeriodicJogRepeat("Y-" + GetMovementQuantity());
    });
    InitPicButton("#jogPlus", function () {
        var cmd = GetCurrentAxis() + "+" + GetMovementQuantity();
        IssueCommand("jog", cmd);
        if (GetMovementQuantity() == 0) PeriodicJogRepeat(cmd);
    });
    InitPicButton("#jogMinus", function () {
        var cmd = GetCurrentAxis() + "-" + GetMovementQuantity();
        IssueCommand("jog", cmd);
        if (GetMovementQuantity() == 0) PeriodicJogRepeat(cmd);
    });
    $("#jfire").click(function () {
        IssueCommand("spindle", "toogle");
    }); /* Home search buttons */
    $("#homebtnpanel .btn").mousedown(function () {
        $(this).attr("style", "background-image: url('../res/b2.png');");
        IssueCommand("homesearch", $(this).text());
    }).mouseup(function () {
        $(this).attr("style", "");
    }); /* fixture offset management */
    $("#fixtureselect .btn").mousedown(function () {
        $(this).attr("style", "background-image: url('../res/b2.png');");
        var id = $(this).attr("id");
        IssueCommand(id, GetCurrentFixtureIndex());
    }).mouseup(function () {
        $(this).attr("style", "");
    }); /* Etc button group */
    $("#gohome").mousedown(function () {
        $(this).attr("style", "background-image: url('../res/b2.png');");
        var id = $(this).attr("id");
        IssueCommand(id, "0");
    }).mouseup(function () {
        $(this).attr("style", "");
    }); /* Program stop button */
    $("#pgmstopbtn").mousedown(function () {
        $(this).attr("style", "background-image: url('../res/b2.png');");
        IssueCommand("program_stop", "0");
    }).mouseup(function () {
        $(this).attr("style", "");
    }); /* Program start/pause/resume button */
    /*
     * interp_state: 1 - Idle
     * 2 - Reading
     * 3 - Paused
     * 4 - Waiting
     */
    $("#pgmstartbtn").mousedown(function () {
        $(this).attr("style", "background-image: url('../res/b2.png');");
        if (typeof last_status.interp_state != 'unknown') {
            if (last_status.interp_state == 1) {
                IssueCommand("foverride", feedOverride);
                IssueCommand("program_start", $("#pgmlineno").val());
            } else if (last_status.interp_state == 3) {
                IssueCommand("program_resume", "0");
            } else {
                IssueCommand("program_pause", "0");
            }
        }
    }).mouseup(function () {
        $(this).attr("style", "");
    }); /* Program step button */
    $("#pgmstepbtn").mousedown(function () {
        $(this).attr("style", "background-image: url('../res/b2.png');");
        IssueCommand("program_step", "0");
    }).mouseup(function () {
        $(this).attr("style", "");
    }); /* Program Open button */
    $("#pgmfile").mousedown(function () {
        $(this).attr("style", "background-image: url('../res/b2.png');");
        $("#pgmbox").hide();
        $("#filebox").show();
        if (!miniemc2Init.filelist_inited) {
            $('#filebox').fileTree({
                root: '',
                script: 'send_cmd',
                expandSpeed: 0,
                collapseSpeed: 0,
                multiFolder: false
            }, function (file) {
                IssueCommand("program_load", file);
                $("#pgmbox").show();
                $("#filebox").hide();
            });
            miniemc2Init.filelist_inited = true;
        }
    }).mouseup(function () {
        $(this).attr("style", "");
    }); /* Program Show button */
    $("#pgmupload").mousedown(function () {
        $(this).attr("style", "background-image: url('../res/b2.png');");
        $("#pgmbox").show();
        $("#filebox").hide();
    }).mouseup(function () {
        $(this).attr("style", "");
    }); /* MDI exec button */
    $("#mdiexec").mousedown(function () {
        $(this).attr("style", "background-image: url('../res/b2.png');");
        SendMdi();
    }).mouseup(function () {
        $(this).attr("style", "");
    });
    $("#reloadbtn").click(function () {
        IssueCommand("reload", "");
    });
    $("#rebootbtn").click(function () {
        IssueCommand("reboot", "");
    });
    $("#applybtn").click(function () {
        var ip = $("#ipline").attr("value");
        if (ip != window.location.host) {
            if (fnValidateIPAddress(ip)) {
                IssueCommand("set_ip", ip);
            } else {
                alert("Invalid IP " + ip);
            }
        }
    });
    $("#posmode").click(function () {
        var id = $(this).attr("id");
        IssueCommand(id, "0");
    });
    // Set IP address line
    $("#ipline").attr("value", window.location.host);
    $("#limitoverride").click(function () {
        var id = $(this).attr("id");
        IssueCommand(id, "0");
    });
    miniemc2Init.sl_pos = $("#feed_slider").offset().top;
    miniemc2Init.sl_height = $("#feed_slider").height() - 19;
    $("#feed_slider").click(function (event) {
        var delta = event.pageY - miniemc2Init.sl_pos;
        if (delta > miniemc2Init.sl_height) delta = miniemc2Init.sl_height;
        $(".sldrag", this).css("top", delta);
        var mult = 3 * (miniemc2Init.sl_height - delta) / miniemc2Init.sl_height;
        feedOverride = parseInt(100 * mult);
        $("#feed_override_value").text(feedOverride + "%");
        IssueCommand("foverride", feedOverride);
    }); /* Main menu functions*/
    $("#topmenu li").click(function () {
        $("#topmenu li.current").removeClass("current");
        $(this).addClass("current");
        ShowPanel($(this).index());
        //alert($(this).index());
    });
    loadConfig();
} /******* Validate IP Address IPv4 *********/

function fnValidateIPAddress(ipaddr) {
    //Remember, this function will validate only Class C IP.
    //change to other IP Classes as you need
    ipaddr = ipaddr.replace(/\s/g, "") //remove spaces for checking
    var re = /^\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}$/; //regex. check for digits and in
    //all 4 quadrants of the IP
    if (re.test(ipaddr)) {
        //split into units with dots "."
        var parts = ipaddr.split(".");
        //if the first unit/quadrant of the IP is zero
        if (parseInt(parseFloat(parts[0])) == 0) {
            return false;
        }
        //if the fourth unit/quadrant of the IP is zero
        if (parseInt(parseFloat(parts[3])) == 0) {
            return false;
        }
        //if any part is greater than 255
        for (var i = 0; i < parts.length; i++) {
            if (parseInt(parseFloat(parts[i])) > 255) {
                return false;
            }
        }
        return true;
    } else {
        return false;
    }
}
var feedOverride = 100;

function SendMdi() {
    IssueCommand("foverride", feedOverride);
    IssueCommand("exec_mdi", $("#mdiline").val());
}

function ShowPanel(index) {
    $(".panel").each(function (indx) {
        if (index == indx) $(this).show();
        else $(this).hide();
    }); /* Handle OnPanelShow event */
    switch (index) {
    case 2:
        /* REquest program's part If interpret not in the IDLE state */
        if (typeof last_status != 'undefined') {
            DoProgramUpdate.force = true;
        }
    }
}

function moveScroll(offset) {
    //$("#pgmbox").scrollTop((offset-1)*15);
    $(".pgmcombo").scrollTop((offset - 1) * 15);
}

function HighLightLine(index) {
    $(".lval.current").removeClass("current");
    var el = $(".lval").get(index);
    $(el).addClass("current");
}

function SetCurrentLine(index) {
    if (index == 0) index = 1;
    HighLightLine(index - 1);
    $("#pgmlineno").val((DoProgramUpdate.context_index * DoProgramUpdate.context_size + index).toString());
}

function DoProgramUpdate(reply) {
    if ($("#auto").css("display") != "none") { /* Update Program Name string */
        $("#pgmname").text("program name:" + reply.last_program);
        /* Update Run/Pause picture
         * interp_state: 1 - Idle
         * 2 - Reading
         * 3 - Paused
         * 4 - Waiting
         */
        var play = $($("#pgmstartbtn img").get(0));
        var pause = $($("#pgmstartbtn img").get(1));
        var resume = $($("#pgmstartbtn img").get(2));
        if (reply.interp_state == 1) {
            // Display Start button when at Idle or Pause state
            play.show();
            pause.hide();
            resume.hide();
        } else if (reply.interp_state == 3) {
            play.hide();
            pause.hide();
            resume.show();
        } else {
            play.hide();
            pause.show();
            resume.hide();
        } /* Update program */
        //if( DoProgramUpdate.last_program != reply.last_program || DoProgramUpdate.context_index != reply.context_index || reply.context_changed)
        if (reply.context_changed || DoProgramUpdate.force || DoProgramUpdate.context_index != reply.context_index) {
            DoProgramUpdate.context_size = reply.context_size;
            DoProgramUpdate.context_index = reply.context_index;
            DoProgramUpdate.last_program = reply.last_program;
            DoProgramUpdate.force = false;
            IssueCommand("get_program", 0);
        } else if (DoProgramUpdate.context_offset != reply.context_offset) {
            DoProgramUpdate.context_offset = reply.context_offset;
            SetCurrentLine(reply.context_offset);
            moveScroll(reply.context_offset - 1);
        }
    }
}

function UpdateProgramPlace(StringList) {
    var nhtml = "";
    for (var i = 0; i < StringList.program.length; i++) {
        nhtml += "<div class=\"pgmtd\"><div class=\"lno\">" + (DoProgramUpdate.context_index * DoProgramUpdate.context_size + i + 1).toString();
        nhtml += "</div><div class=\"lval\">" + StringList.program[i];
        nhtml += "</div></div>\n";
    }
    $("#pgmbox").html(nhtml);
    $(".pgmtd").click(function () {
        var idx = $(this).index();
        SetCurrentLine(idx + 1);
    });
} /* LIST management functions */

function InitList(id, prev, next) {
    var selector = id + " .arrow_right";
    var jq = $(selector);
    jq.mousedown(function () {
        $(this).addClass("pressed");
        if (prev) $(id + " .list_value").text(prev());
    }).mouseup(function () {
        $(this).removeClass("pressed");
    });
    $(id + " .arrow_left").mousedown(function () {
        $(this).addClass("pressed");
        if (next) $(id + " .list_value").text(next());
    }).mouseup(function () {
        $(this).removeClass("pressed");
    });
}

function SetListValue(id, value) {
    $(id + " .list_value").text(value);
}

function InitPicButton(sel, callback) {
    $(sel).mousedown(function () {
        var x = $("img", this);
        callback();
        x[0].style.visibility = "hidden";
        x[1].style.visibility = "visible";
    }).mouseup(function () {
        StopPeriodicJogRepeat();
        var x = $("img", this);
        x[1].style.visibility = "hidden";
        x[0].style.visibility = "visible";
    });
} /* Movement display and control functions */

function StopPeriodicJogRepeat() {
    PeriodicJogRepeat.started = false;
}

function _PeriodicJog(cmd) {
    IssueCommand("jog_repeat", cmd);
    if (PeriodicJogRepeat.started) PeriodicJogRepeat.timeoutId = window.setTimeout("_PeriodicJog(\"" + cmd + "\")", 100);
}

function PeriodicJogRepeat(cmd) {
    if (PeriodicJogRepeat.started) {
        //Cancel current transaction
        clearTimeout(PeriodicJogRepeat.timeoutId);
    }
    PeriodicJogRepeat.cmd = cmd;
    PeriodicJogRepeat.timeoutId = window.setTimeout("_PeriodicJog(\"" + cmd + "\")", 100);
    PeriodicJogRepeat.started = true;
}
var FixtureList = ["G54", "G55", "G56", "G57", "G58", "G59", "G59.1", "G59.2"];
var CurrentFixture = 0;

function GetCurrentFixtureIndex() {
    return CurrentFixture;
}

function NextFixture() {
    if (++CurrentFixture >= FixtureList.length) CurrentFixture = 0;
    return FixtureList[CurrentFixture];
}

function PrevFixture() {
    if (--CurrentFixture < 0) CurrentFixture = FixtureList.length - 1;
    return FixtureList[CurrentFixture];
}
var JogIncr = ["cont.", "500.0", "100.0", "10.00", "5.000", "1.000", "0.100", "0.010", "0.001"];
var JogIncrPos = 0;

function GetMovementQuantity() {
    var x = JogIncr[JogIncrPos];
    if (JogIncrPos == 0) x = "0";
    return x;
}

function NextMovementQauntity() {
    JogIncrPos++;
    if (JogIncrPos >= JogIncr.length) JogIncrPos = 0;
    return JogIncr[JogIncrPos];
}

function PrevMovementQauntity() {
    JogIncrPos--;
    if (JogIncrPos < 0) JogIncrPos = JogIncr.length - 1;
    return JogIncr[JogIncrPos];
} /* Axis list functions */
var axisList = [];
var axisListPos = 0;

function GetCurrentAxis() {
    return axisList[axisListPos];
}

function DispNextAxisName() {
    do {
        axisListPos++;
        if (axisListPos >= axisList.length) axisListPos = 0;
    } while (axisList[axisListPos] == "X" || axisList[axisListPos] == "Y");
    return axisList[axisListPos];
}

function DispPrevAxisName() {
    do {
        axisListPos--;
        if (axisListPos < 0) axisListPos = axisList.length - 1;
    } while (axisList[axisListPos] == "X" || axisList[axisListPos] == "Y");
    return axisList[axisListPos];
} /* Spindle management functions */

function Spindle(val) {
    var imgs = $("#jfire img");
    if (val) {
        imgs[0].style.visibility = "hidden";
        imgs[1].style.visibility = "visible";
    } else {
        imgs[1].style.visibility = "hidden";
        imgs[0].style.visibility = "visible";
    }
} /* EStop management functions */

function SetEStopAlarm(val) {
    EStop.alarm = val;
}

function EStop(val) {
    if (!val) {
        if (EStop.alarm) {
            $("#estop").removeClass("estop_pressed");
            $("#estop").removeClass("estop_released");
            $("#estop").addClass("estop_alarm");
        } else {
            $("#estop").removeClass("estop_pressed");
            $("#estop").removeClass("estop_alarm");
            $("#estop").addClass("estop_released");
        }
    } else {
        $("#estop").removeClass("estop_alarm");
        $("#estop").removeClass("estop_released");
        $("#estop").addClass("estop_pressed");
    }
}

function IssueCommand(cmd, value) {
    IssueCommand.active_cmd = cmd;
    $.ajax({
        url: "send_cmd",
        data: cmd + "=" + value,
        dataType: 'json',
        cache: false,
        timeout: 30000,
        complete: function (jqXHR, textStatus) {
            if (textStatus != 'success') {
                //console.log("Last req status is " + textStatus + ",restarting");
                $.unblockUI();
            }
        },
        success: function (data) {
            if (typeof data != 'undefined') {
                if (IssueCommand.active_cmd == "get_program") UpdateProgramPlace(data);
                if (IssueCommand.active_cmd == "reload") {
                    $.blockUI({
                        message: 'Reloading...'
                    });
                    window.setTimeout(function () {
                        $.unblockUI();
                        window.location.reload();
                    }, 15000);
                }
                if (IssueCommand.active_cmd == "reboot") {
                    $.blockUI({
                        message: 'Rebooting...'
                    });
                    window.setTimeout(function () {
                        $.unblockUI();
                        window.location.reload();
                    }, 50000);
                }
 
                if (IssueCommand.active_cmd == "set_ip") {
                    $.blockUI({
                        message: 'Changing IP...'
                    });
                    window.setTimeout(function () {
                        $.unblockUI();
                        var ip = $("#ipline").attr("value");
                        window.location.replace( "http://" + ip );
                    }, 7000 );
                }                
                
            }
        }
    });
}

function loadConfig() {
    $.ajax({
        url: 'data/emc_config.json',
        dataType: 'json',
        cache: false,
        complete: function (jqXHR, textStatus) {
            if (textStatus != 'success') {
                //console.log("Last req status is " + textStatus + ",restarting");
                window.setTimeout(loadConfig, 2000);
            }
        },
        success: function (data) {
            try {
                // Initialyzing axes position elements
                var homeBtns = $("#homebtnpanel .btn");
                for (var i = 0; i < data.axes.length; i++) {
                    if (!data.axes[i].used || data.axes[i].slave_for != '-') {
                        $("#axis_" + i).hide();
                        $(homeBtns[i]).hide();
                    } else {
                        //Adding axis to the list
                        $("#axis_" + i).show();
                        $(homeBtns[i]).show();
                        axisList[axisListPos++] = data.axes[i].name;
                    }
                }
                //Update Axis selection list
                SetListValue("#axes_list", DispNextAxisName());
                //Setup EStop behavior
                $("#estop").click(function () {
                    if ($(this).hasClass("estop_pressed")) {
                        //EStop(false);
                        IssueCommand("power", "off");
                    } else {
                        //EStop(true);
                        IssueCommand("power", "on");
                    }
                });
                dynamicUpdate();
            } catch (e) {}
        }
    });
}

function addError(error) {
    setLowLine("Error: " + error, "red");
}

function addWarn(error) {
    setLowLine("Warn: " + error, "");
}

function addInfo(error) {
    setLowLine("Info: " + error, "green");
}

function setLowLine(error, xclass) {
    setLowLine.lastClass = xclass;
    $("#errline").addClass(xclass).text(error);
    window.setTimeout(function () {
        $("#errline").text("").removeClass(setLowLine.lastClass);
    }, 3000);
}
var last_status;
var ctrl_modes = ["Unknown", "Manual", "Auto", "MDI"];
var interp_modes = ["Unknown", "Idle", "Reading", "Pause", "Waiting"];

function dynamicUpdate() {
    dynamicUpdate.reqQueued = false;
    $.ajax({
        url: 'data/emc_dynamic.json',
        dataType: 'json',
        cache: false,
        complete: function (jqXHR, textStatus) {
            if (!dynamicUpdate.reqQueued) {
                //console.log("Last req status is " + textStatus + ",restarting");
                window.setTimeout(dynamicUpdate, 250);
                dynamicUpdate.reqQueued = true;
            }
        },
        success: function (data) {
            try {
                if (typeof data != 'unknown') {
                    last_status = data; /* Update status line */
                    $("#ctlmtext").text(ctrl_modes[data.ctrl_mode]);
                    $("#imtext").text(interp_modes[data.interp_state]);
                    if (!data.inpos) {
                        $(".inpos").addClass("move");
                    } else {
                        $(".inpos").removeClass("move");
                    } /* Update axes positions */
                    for (var i = 0; i < data.positions.length; i++) {
                        $("#axis_" + i + " .text_coord").text(data.positions[i].toFixed(4));
                    }
                    $("#feed_value").text((data.feed * 60).toFixed(1)); /* Update error string */
                    if (data.errors) {
                        addError(data.errors[0]);
                    } /* Update warning string */
                    if (data.warn) {
                        addWarn(data.warn[0]);
                    } /* Update info string */
                    if (data.info) {
                        addInfo(data.info[0]);
                    }
                    //E-Stop status changing ctrl_state == 4 if Machine is on
                    SetEStopAlarm(data.ctrl_state == 1);
                    if (data.ctrl_state != 4) {
                        EStop(false);
                    } else {
                        EStop(true);
                    }
                    //Spindle status
                    Spindle(data.spindle_on);
                    // Homes panel
                    var axes = ["X", "Y", "Z", "A", "B", "C"];
                    if ($("#homes").css("display") != "none") { /* Offsets values */
                        $(".offvalue").each(function (index) {
                            $(this).text(axes[index] + ": " + data.offsets[index].toPrecision(4));
                        }); /* Modal codes */
                        $("#modalcodes").text(data.active_codes); /* Position mode button switching */
                        if (dynamicUpdate.lastPosMode != data.pos_abs) {
                            if (data.pos_abs) {
                                $("#posmode").text("Absolute");
                                $("#posmode").attr("style", "");
                            } else {
                                $("#posmode").attr("style", "background-image: url('../res/b2.png');");
                                $("#posmode").text("Relative");
                            }
                            dynamicUpdate.lastPosMode = data.pos_abs;
                        } /* Limit override button switching */
                        if (dynamicUpdate.lastLimitOverride != data.limit_over) {
                            if (!data.limit_over) {
                                $("#limitoverride").text("Use limit");
                                $("#limitoverride").attr("style", "");
                            } else {
                                $("#limitoverride").attr("style", "background-image: url('../res/b2.png');");
                                $("#limitoverride").text("Overriden");
                            }
                            dynamicUpdate.lastLimitOverride = data.limit_over;
                        }
                    }
                    // Update program window
                    DoProgramUpdate(data);
                }
            } catch (e) { /*console.log(e);*/
            }
            window.setTimeout(dynamicUpdate, 250);
            dynamicUpdate.reqQueued = true;
        }
    });
}