<?php

include("common.php");

if (!isset($_REQUEST["key"])) {
   print "No project key specified.\n";
   die();
}

$key = $_REQUEST["key"];

$PROJECT_DIR = $PROJECTS_PATH."/".$key;
$PROJECT_JSON = $PROJECT_DIR."/"."project.json";
$PROJECT_URL = $PROJECTS_URL."/".$key;

	         do_header("Project editor");

print "okay";

?>
<table id=slidetable>
</table>

<script>
_slides = JSON.parse('["slide-0.png","slide-1.png","slide-2.png","slide-3.png","slide-4.png","slide-5.png","slide-6.png","slide-7.png","slide-8.png"]');
table = document.getElementById("slidetable");

while(table.rows.length < _slides.length)
 table.insertRow(-1);

project_url = "<?php echo $PROJECT_URL ?>";

for (var i = 0; i < _slides.length; i++) {
    var row = table.rows[i];

    row.insertCell(-1);

    row.cells[0].innerHTML="<img src="+project_url+"/"+_slides[i].replace("slide","preview")+">";

    console.log(_slides[i]);


}

</script>


<?php
	do_footer();
?>