import * as cdk from 'aws-cdk-lib';
import { Template } from 'aws-cdk-lib/assertions';
import { DeviceStack } from '../lib/device-stack';

const synth = (deployEnv: 'sandbox' | 'prod', deviceNames: string[]) => {
  const app = new cdk.App();
  const stack = new DeviceStack(app, 'Test', {
    env: { account: '000000000000', region: 'eu-west-2' },
    deployEnv,
    deviceNames,
  });
  return Template.fromStack(stack);
};

describe('DeviceStack', () => {
  test('one Thing per device, with the attributes Fleet reads', () => {
    const t = synth('sandbox', ['unit-1', 'unit-2']);
    t.resourceCountIs('AWS::IoT::Thing', 2);
    t.hasResourceProperties('AWS::IoT::Thing', {
      ThingName: 'controller-stand-sandbox-unit-1',
      AttributePayload: {
        Attributes: { project: 'controller-stand', deviceType: 'charging-stand', capabilities: 'ota,ota-signed' },
      },
    });
  });

  test('the policy pins each device to its own client id and topics', () => {
    const t = synth('prod', ['unit-1']);
    const policies = t.findResources('AWS::IoT::Policy');
    const [policy] = Object.values(policies) as { Properties: { PolicyName: string; PolicyDocument: unknown } }[];
    expect(policy.Properties.PolicyName).toBe('controller-stand-device-prod');
    const doc = JSON.stringify(policy.Properties.PolicyDocument);
    expect(doc).toContain('client/${iot:Connection.Thing.ThingName}');
    expect(doc).toContain('iot:Connection.Thing.IsAttached');
    expect(doc).toContain('topic/controller-stand/${iot:Connection.Thing.ThingName}/responses');
    expect(doc).toContain('topic/controller-stand/${iot:Connection.Thing.ThingName}/status');
    expect(doc).toContain('topicfilter/controller-stand/${iot:Connection.Thing.ThingName}/commands');
    // Nothing broader than its own topics.
    expect(doc).not.toMatch(/topic\/controller-stand\/\*|topic\/\*|"iot:\*"/);
  });

  test('no devices: just the policy', () => {
    const t = synth('prod', []);
    t.resourceCountIs('AWS::IoT::Thing', 0);
    t.resourceCountIs('AWS::IoT::Policy', 1);
  });
});
