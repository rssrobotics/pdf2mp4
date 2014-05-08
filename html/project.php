<?php

include("common.php");

do_header("Project editor");
do_banner();


$key = $_REQUEST["key"];

if (strlen($key) != 40 || !preg_match("/^[a-z0-9]*$/", $key)) {
print "<h2>Sorry, invalid key.</h2>";
do_footer();
   die();
}

$PROJECT_DIR = $PROJECTS_PATH."/".$key;
$PROJECT_JSON = $PROJECT_DIR."/"."project.json";
$PROJECT_URL = $PROJECTS_URL."/".$key;

?>

<!-- the primary slide display area with the larger thumbnail and arrow controls -->
<center>
<table border=0 width=100%>
  <tr>
    <td><!-- the tiny preview slides will go in here -->
      <center>
    <div id=slide_nav>
    </div>
    </center>

    <td valign=top>
      <center>
    <div id=thumb_area>
      <img id=current_slide class=thumb><br>
      <center>
      <div id=controls_area>
	<img id=prev_button src=prev_button.png><span id=current_slide_label></span>
	<img id=next_button src=next_button.png><br><br>
	Seconds: <input type=text id=slide_seconds size=5><br><br>
	Show progress bar: <input id=slide_progress_bar type=checkbox>
	</center>
      </div>
    </div>
    </center>

    <td valign=top>
    <div id=status_div>
      Project name: <input type=text id=project_name><br><br>
      Status: <span id=status_label></span><br><br>
      Total seconds: <span id=total_seconds>0</span><br>
      (RSS 2014 maximum 300)<br><br>
      Show overall progress bar: <input id=global_progress_bar type=checkbox><br><br>
      Render preview video<br><br>
      Render final video<br><br>
    </div>
  </tr>
</table>

</center>

<script>
var project_url = "<?php echo $PROJECT_URL ?>";
var key = "<?php echo $key ?>";

<?php
$fd = fopen($PROJECT_DIR."/json.txt", "r");
if (!$fd) {
  // new document
  print "doc_create(8);\n";
} else {
 $s = fgets($fd);
 print "doc_restore('$s');\n";
}

?>
</script>


<?php
do_footer();
?>
