class mx.transitions.easing.None
{
   static var SKYUI_RELEASE_IDX = 16;
   static var SKYUI_VERSION_MAJOR = 4;
   static var SKYUI_VERSION_MINOR = 1;
   static var SKYUI_VERSION_STRING = mx.transitions.easing.None.SKYUI_VERSION_MAJOR + "." + mx.transitions.easing.None.SKYUI_VERSION_MINOR;
   function None()
   {
   }
   static function easeNone(t, b, c, d)
   {
      return c * t / d + b;
   }
   static function easeIn(t, b, c, d)
   {
      return c * t / d + b;
   }
   static function easeOut(t, b, c, d)
   {
      return c * t / d + b;
   }
   static function easeInOut(t, b, c, d)
   {
      return c * t / d + b;
   }
}
