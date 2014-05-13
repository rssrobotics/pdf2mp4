<?php

include("common.php");
do_header("pdf2mp4");
do_banner();

?>

<center>
<table>
<tr><td width=50% valign=top class=topmenu><h2>Create a new project</h2></P>

<form enctype="multipart/form-data" method="post" action=project_new_submit.php>
<input type=file size=50 name=pdffile><br><br>
<input type=submit>
<input type=hidden name="key" value="<?php echo $key ?>">
</form>
<br>
(Maximum size 100MB)

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
