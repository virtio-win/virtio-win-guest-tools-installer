﻿//------------------------------------------------------------------------------
// <auto-generated>
//     This code was generated by a tool.
//     Runtime Version:4.0.30319.42000
//
//     Changes to this file may cause incorrect behavior and will be lost if
//     the code is regenerated.
// </auto-generated>
//------------------------------------------------------------------------------

using System.Xml.Serialization;

// 
// This source code was auto-generated by xsd, Version=4.8.4161.0.
// 


/// <remarks/>
[System.CodeDom.Compiler.GeneratedCodeAttribute("xsd", "4.8.4161.0")]
[System.SerializableAttribute()]
[System.Diagnostics.DebuggerStepThroughAttribute()]
[System.ComponentModel.DesignerCategoryAttribute("code")]
[System.Xml.Serialization.XmlTypeAttribute(AnonymousType=true, Namespace="http://www.redhat.com/DrvInstExt")]
[System.Xml.Serialization.XmlRootAttribute(Namespace="http://www.redhat.com/DrvInstExt", IsNullable=false)]
public partial class DriverInstall {
    
    private YesNoType deleteFilesField;
    
    private bool deleteFilesFieldSpecified;
    
    private string flagsField;
    
    private string sequenceField;
    /// <remarks/>
    [System.Xml.Serialization.XmlAttributeAttribute()]
    public YesNoType DeleteFiles {
        get {
            return this.deleteFilesField;
        }
        set {
            this.deleteFilesField = value;
        }
    }
    
    /// <remarks/>
    [System.Xml.Serialization.XmlIgnoreAttribute()]
    public bool DeleteFilesSpecified {
        get {
            return this.deleteFilesFieldSpecified;
        }
        set {
            this.deleteFilesFieldSpecified = value;
        }
    }

    /// <remarks/>
    [System.Xml.Serialization.XmlAttributeAttribute(DataType="integer")]
    public string Flags {
        get {
            return this.flagsField;
        }
        set {
            this.flagsField = value;
        }
    }

    /// <remarks/>
    [System.Xml.Serialization.XmlAttributeAttribute(DataType="integer")]
    public string Sequence {
        get {
            return this.sequenceField;
        }
        set {
            this.sequenceField = value;
        }
    }
}

/// <remarks/>
[System.CodeDom.Compiler.GeneratedCodeAttribute("xsd", "4.8.4161.0")]
[System.SerializableAttribute()]
[System.Xml.Serialization.XmlTypeAttribute(Namespace="http://www.redhat.com/DrvInstExt")]
public enum YesNoType {
    
    /// <remarks/>
    no,
    
    /// <remarks/>
    yes,
}
