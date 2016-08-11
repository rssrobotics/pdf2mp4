<?php

include("common.php");
do_header("pdf2mp4");
do_banner();

?>

<?php include("googlead.php"); ?>

<center>
<table>
<tr><td width=50% valign=top class=topmenu><h2>Create a new project</h2></P>

<form enctype="multipart/form-data" method="post" action=project_new_submit.php>

Preset:
<select name="profile">
<option value="height=1080;aspect=1.777;pheight=320;fps=30;pfps=1;bitrate=8000000;pbitrate=2000000;maxtime=300">1920x1080, 300 seconds</option>
<option value="height=1080;aspect=1.333;pheight=320;fps=30;pfps=1;bitrate=8000000;pbitrate=2000000;maxtime=300">1440x1080, 300 seconds</option>
</select><br><br>
    PDF: <input type=file size=50 name=pdffile accept='.pdf'><br>
    <font size=-1>(Maximum size 100MB)</font><br><br>
    <input type=image src=upload.png name=submit>


</form>
<br>


<td valign=top class=topmenu>
<h2>Go to an existing project:</h2>
<div id=existing_projects_div></div>

<script>
var projects = JSON.parse(localStorage.getItem("projects"));

if (projects != null) {
    for (var key in projects) {
        var name = projects[key].name;

        if (name == null)
            continue;

        document.getElementById("existing_projects_div").innerHTML += "<a href=project.php?key="+key+">"+projects[key].name+"</a><br><font size=-1>key: "+key+"<br>"+"Created: "+projects[key].create_date+"<br><br></font>";
    }
}

function on_keypress(e)
{
    if (e && e.keyCode == 13)
        on_manual_key();
}

function on_manual_key()
{
    var key = document.getElementById("key").value;

    document.location="project.php?key="+key;
}

</script>

<P>Manual key entry:<br><input id=key onkeyup='on_keypress(event)'  type=text name=key size=40><input onclick='on_manual_key()' type=submit value="Go!">
</table>
</center>

<?php
do_footer();
?>
