/*
 * Copyright (C) 2024 Red Hat, Inc.
 *
 * Written By: Vadim Rozenfeld <vrozenfe@redhat.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met :
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and / or other materials provided with the distribution.
 * 3. Neither the names of the copyright holders nor the names of their contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
namespace DrvInstExt
{
    using System;
    using System.Collections;
    using System.Reflection;
    using System.Xml;
    using System.Xml.Schema;
    using Microsoft.Tools.WindowsInstallerXml;

    public sealed class DrvInstallerCompiler : CompilerExtension
    {
        private XmlSchema schema;
        private readonly Hashtable components;

        public DrvInstallerCompiler()
        {
            this.components = new Hashtable();
        }
        public override XmlSchema Schema
        {
            get
            {
                if (null == this.schema)
                {
                    this.schema = LoadXmlSchemaHelper(Assembly.GetExecutingAssembly(), "DrvInstExt.WinDrvSchema.xsd");
                }
                return this.schema;
            }
        }
        public override void ParseElement(SourceLineNumberCollection sourceLineNumbers, XmlElement parentElement, XmlElement element, params string[] contextValues)
        {
            switch (parentElement.LocalName)
            {
                case "Component":
                    string componentId = contextValues[0];

                    switch (element.LocalName)
                    {
                        case "DriverInstall":
                            this.ParseDriverElement(element, componentId);
                            break;
                        default:
                            this.Core.UnexpectedElement(parentElement, element);
                            break;
                    }
                    break;
                default:
                    this.Core.UnexpectedElement(parentElement, element);
                    break;
            }
        }
        private void ParseDriverElement(XmlNode node, string componentId)
        {
            SourceLineNumberCollection sourceLineNumbers = Preprocessor.GetSourceLineNumbers(node);
            int attributes = 0;
            string filename = null;
            string directory = null;
            int sequence = 0;

            if (null != componentId)
            {
                if (this.components.Contains(componentId))
                {
                    this.Core.OnMessage(WixErrors.TooManyElements(sourceLineNumbers, "Component", node.Name, 1));
                }
                else
                {
                    this.components.Add(componentId, null);
                }
            }

            foreach (XmlAttribute attrib in node.Attributes)
            {
                switch (attrib.LocalName)
                {
                    case "FileName":
                        filename = this.Core.GetAttributeValue(sourceLineNumbers, attrib);
                        break;
                    case "Directory":
                        directory = this.Core.GetAttributeValue(sourceLineNumbers, attrib);
                        break;
                    case "Sequence":
                        sequence = this.Core.GetAttributeIntegerValue(sourceLineNumbers, attrib, 0, int.MaxValue);
                        break;
                    default:
                        this.Core.UnexpectedAttribute(sourceLineNumbers, attrib);
                        break;
                }
            }

            if (String.IsNullOrEmpty(filename))
            {
                this.Core.OnMessage(WixErrors.ExpectedAttribute(sourceLineNumbers, node.Name, "filename"));
            }

            if (String.IsNullOrEmpty(directory))
            {
                this.Core.OnMessage(WixErrors.ExpectedAttribute(sourceLineNumbers, node.Name, "directory"));
            }

            if (!this.Core.EncounteredError)
            {
                Row row = this.Core.CreateRow(sourceLineNumbers, "DriverInstallTable");
                row[0] = componentId;
                row[1] = filename;
                row[2] = directory;
                row[3] = attributes;
                if (CompilerCore.IntegerNotSet != sequence)
                {
                    row[4] = sequence;
                }
                this.Core.CreateWixSimpleReferenceRow(sourceLineNumbers, "CustomAction", "ProcessDriverPackages");
            }
        }
    }
}
