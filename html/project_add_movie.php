<?php

include("common.php");

do_header("Project editor");
do_banner();

validate_key($key);

?>

<form enctype="multipart/form-data" method=post action=project_add_movie_submit.php>
<input type=hidden name=key value="<?php print $key ?>">
<input type=hidden name=idx value="<?php print $_REQUEST["idx"] ?>">
Movie: <input type=file size=50 name=moviefile><br>
Play rate (fps): <input type=text name=playfps value=30 size=4><br>
Total seconds: <input type=text name=seconds value=4 size=4><br>
Play mode:
<select name=mode>
  <option value=hold>Hold last frame</option>
  <option value=loop>Loop video</option>
</select>

<br>
<input type=submit>

</form>

<?php do_footer(); ?>
