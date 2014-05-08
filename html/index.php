<?php

include("common.php");
do_banner();

?>

<table>
<tr><td width=50% valign=top><a href=project_new.php><h2>Create a new project</h2></a></P>

<td valign=top>
<h3>Go to an existing project:</h3>
<div id=existing_projects_div></div>

<script>
var projects = JSON.parse(localStorage.getItem("projects"));

if (projects != null) {
    for (var key in projects) {
	var name = projects[key].name;

	document.getElementById("existing_projects_div").innerHTML += "<a href=project.php?key="+key+">"+projects[key].name+"</a><br><font size=-1>key: "+key+"<br>"+"Created: "+projects[key].create_date+"<br><br></font>";
    }
}

function on_manual_key()
{
    var key = document.getElementById("key").value;

    document.location="project.php?key="+key;
}

</script>

<P>Key: <input id=key type=text name=key size=40><input onclick='on_manual_key()' type=submit value="Go!">
</table>

<?php
do_footer();
?>
