# 6/4/2026 9 AM - Research and project idea

_Time spent: 1h_

Basically did some research and thinking about this project. I think it has the potential to be really cool. I sent my musings in slack to ask for feedback, but here's how it's feeling so far:

I think it'd be cool to make my own airtags. There exists openhaystack firmware to connect to apple's mesh, so this is feasible at least in theory, plus there already exist third party airtags so others have done it at least
In terms of complexity, I'd imagine it's at or above a devboard.
One hard part is going to be power mangement

In terms of capable chips, I see two options: the esp32 and the nrf. Both have openhaystack ports.

esps have ble soc, and I think the c3 is pretty stripped down and optimized. It draws maybe 150 ua, so about six months on an AAA battery, pretty good

There's also the nrf, which is more "professional grade" I guess. It has a harder toolchain, but draws much less current. Even on a tiny cr2032 coin battery, it'd last a really long time. I'm leaning towards the c3 though.



With regards to that, I also have the option of using the raw qfn chip or using the package. Both fit within spatial budget.


What I'm leaning towards now is esp32c3 qfn, off the shelf external antenna, AAA battery. It's not quite the professional form factor of the coin cell x nrf, but honestly it has some charm to it. qfn feels like just the right amount of challenge, and also I want pcba :wesh:
