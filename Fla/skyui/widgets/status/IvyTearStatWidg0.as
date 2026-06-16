class skyui.widgets.status.IvyTearStatWidg0 extends skyui.widgets.WidgetBase
{
   var _bathIcon;
   var iconHolder;
   function IvyTearStatWidg0()
   {
      super();
      this._bathIcon = this.iconHolder.bathIcon;
      this._visible = false;
      gfx.events.EventDispatcher.initialize(this);
   }
   function getWidth()
   {
      return this._width;
   }
   function getHeight()
   {
      return this._height;
   }
   function setVisible(a_visible)
   {
      this._visible = a_visible;
   }
   function setBathColorLevel(a_colorLevel)
   {
      this.iconHolder.gotoAndStop(a_colorLevel);
   }
   function get Scale()
   {
      return this._xscale;
   }
   function set Scale(scale)
   {
      this._xscale = scale;
      this._yscale = scale;
      this.invalidateSize();
   }
}
