
function entryPoint() {
  helper();
}

function helper() {
  var s = "abc";
  var x = 1;
  var y;
  var z = false;
  throw new Error("exception is thrown");
}

entryPoint();
