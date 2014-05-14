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
<input type=hidden name=playfps value=30>
<input type=hidden name=mode value="hold">

<br>
<input type=submit>

</form>

<?php do_footer(); ?>
