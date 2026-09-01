# The show engine
This is the core the processes and keeps all the internal state of DMX. All events should be submitted to it, and it will generate updates to DMX (pushed to DMX buffer) and updates for GUI (polled by rendering code).

That said, it should run in a different thread as GUI and should have a deadline to ensure smooth updates.