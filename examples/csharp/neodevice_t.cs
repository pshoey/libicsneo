//------------------------------------------------------------------------------
// <auto-generated />
//
// This file was automatically generated by SWIG (http://www.swig.org).
// Version 4.0.0
//
// Do not make changes to this file unless you know what you are doing--modify
// the SWIG interface file instead.
//------------------------------------------------------------------------------


public class neodevice_t : global::System.IDisposable {
  private global::System.Runtime.InteropServices.HandleRef swigCPtr;
  protected bool swigCMemOwn;

  internal neodevice_t(global::System.IntPtr cPtr, bool cMemoryOwn) {
    swigCMemOwn = cMemoryOwn;
    swigCPtr = new global::System.Runtime.InteropServices.HandleRef(this, cPtr);
  }

  internal static global::System.Runtime.InteropServices.HandleRef getCPtr(neodevice_t obj) {
    return (obj == null) ? new global::System.Runtime.InteropServices.HandleRef(null, global::System.IntPtr.Zero) : obj.swigCPtr;
  }

  ~neodevice_t() {
    Dispose(false);
  }

  public void Dispose() {
    Dispose(true);
    global::System.GC.SuppressFinalize(this);
  }

  protected virtual void Dispose(bool disposing) {
    lock(this) {
      if (swigCPtr.Handle != global::System.IntPtr.Zero) {
        if (swigCMemOwn) {
          swigCMemOwn = false;
          icsneocsharpPINVOKE.delete_neodevice_t(swigCPtr);
        }
        swigCPtr = new global::System.Runtime.InteropServices.HandleRef(null, global::System.IntPtr.Zero);
      }
    }
  }

  public SWIGTYPE_p_void device {
    set {
      icsneocsharpPINVOKE.neodevice_t_device_set(swigCPtr, SWIGTYPE_p_void.getCPtr(value));
    } 
    get {
      global::System.IntPtr cPtr = icsneocsharpPINVOKE.neodevice_t_device_get(swigCPtr);
      SWIGTYPE_p_void ret = (cPtr == global::System.IntPtr.Zero) ? null : new SWIGTYPE_p_void(cPtr, false);
      return ret;
    } 
  }

  public int handle {
    set {
      icsneocsharpPINVOKE.neodevice_t_handle_set(swigCPtr, value);
    } 
    get {
      int ret = icsneocsharpPINVOKE.neodevice_t_handle_get(swigCPtr);
      return ret;
    } 
  }

  public uint type {
    set {
      icsneocsharpPINVOKE.neodevice_t_type_set(swigCPtr, value);
    } 
    get {
      uint ret = icsneocsharpPINVOKE.neodevice_t_type_get(swigCPtr);
      return ret;
    } 
  }

  public string serial {
    set {
      icsneocsharpPINVOKE.neodevice_t_serial_set(swigCPtr, value);
    } 
    get {
      string ret = icsneocsharpPINVOKE.neodevice_t_serial_get(swigCPtr);
      return ret;
    } 
  }

  public neodevice_t() : this(icsneocsharpPINVOKE.new_neodevice_t(), true) {
  }

}